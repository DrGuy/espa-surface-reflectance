#include "l8_sr.h"

/******************************************************************************
MODULE:  compute_toa_refl

PURPOSE:  Computes the TOA reflectance and at-sensor brightness temps for all
the bands except the pan band.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error computing the reflectance
SUCCESS         No errors encountered

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

HISTORY:
Date          Programmer       Reason
----------    ---------------  -------------------------------------
12/9/2014     Gail Schmidt     Broke the source code into a function to
                               modularize the source code in the main routine

NOTES:
******************************************************************************/
int compute_toa_refl
(
    Input_t *input,     /* I: input structure for the Landsat product */
    uint16 *qaband,     /* I: QA band for the input image, nlines x nsamps */
    int nlines,         /* I: number of lines in reflectance, thermal bands */
    int nsamps,         /* I: number of samps in reflectance, thermal bands */
    float xmus,         /* I: cosine of solar zenith angle */
    char *instrument,   /* I: instrument to be processed (OLI, TIRS) */
    int16 **sband       /* O: output surface reflectance and brightness
                              temp bands */
)
{
    char errmsg[STR_SIZE];                   /* error message */
    char FUNC_NAME[] = "compute_toa_refl";   /* function name */
    int i;               /* looping variable for pixels */
    int ib;              /* looping variable for input bands */
    int sband_ib;        /* looping variable for output bands */
    int iband;           /* current band */
    float rotoa;         /* top of atmosphere reflectance */
    float tmpf;          /* temporary floating point value */
    uint16 *uband = NULL;  /* array for input image data for a single band,
                              nlines x nsamps */

    /* LANDSAT OLI/TIRS constants for offset and scaling */
    const float refl_mult = 2.0E-05; /* reflectance multiplier for bands 1-9 */
    const float refl_add = -0.1;     /* reflectance additive for bands 1-9 */

    /* Radiance offset and scaling for bands 10 and 11, might also be found
       in the MTL file */
    const float xcals = 3.3420E-04;  /* radiance multiplier for bands
                                        10 and 11 */
    const float xcalo = 0.10000;     /* radiance additive for bands
                                        10 and 11 */

    /* K[1|2]b1[0|1] constants might also be found in the MTL file */
    const float k1b10 = 774.89;      /* temperature constant for band 10 */
    const float k1b11 = 480.89;      /* temperature constant for band 11 */
    const float k2b10 = 1321.08;     /* temperature constant for band 10 */
    const float k2b11 = 1201.14;     /* temperature constant for band 11 */

    /* Allocate space for band data */
    uband = calloc (nlines*nsamps, sizeof (uint16));
    if (uband == NULL)
    {
        sprintf (errmsg, "Error allocating memory for uband");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through all the bands (except the pan band) and compute the TOA
       reflectance and at-sensor brightness temp */
    for (ib = DN_BAND1; ib <= DN_BAND11; ib++)
    {
        /* Don't process the pan band */
        if (ib == DN_BAND8)
            continue;
        printf ("%d ... ", ib+1);

        /* Read the current band and calibrate bands 1-9 (except pan) to
           obtain TOA reflectance. Bands are corrected for the sun angle at
           the center of the scene. */
        if (ib <= DN_BAND9)
        {
            if (ib <= DN_BAND7)
            {
                iband = ib;
                sband_ib = ib;
            }
            else
            {  /* don't count the pan band */
                iband = ib - 1;
                sband_ib = ib - 1;
            }

            if (get_input_refl_lines (input, iband, 0, nlines, uband) !=
                SUCCESS)
            {
                sprintf (errmsg, "Reading band %d", ib+1);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            for (i = 0; i < nlines*nsamps; i++)
            {
                /* If this pixel is not fill */
                if (qaband[i] != 1)
                {
                    /* Compute the TOA reflectance based on the scene center sun
                       angle.  Scale the value for output. */
                    rotoa = (uband[i] * refl_mult) + refl_add;
                    rotoa = rotoa * MULT_FACTOR / xmus;

                    /* Save the scaled TOA reflectance value, but make
                       sure it falls within the defined valid range. */
                    if (rotoa < MIN_VALID)
                        sband[sband_ib][i] = MIN_VALID;
                    else if (rotoa > MAX_VALID)
                        sband[sband_ib][i] = MAX_VALID;
                    else
                        sband[sband_ib][i] = (int) rotoa;
                }
                else
                    sband[sband_ib][i] = FILL_VALUE;
            }
        }  /* end if band <= band 9 */

        /* Read the current band and calibrate thermal bands.  Not available
           for OLI-only scenes. */
        else if (ib == DN_BAND10 && strcmp (instrument, "OLI"))
        {
            if (get_input_th_lines (input, 0, 0, nlines, uband) != SUCCESS)
            {
                sprintf (errmsg, "Reading band %d", ib+1);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* Compute brightness temp for band 10.  Make sure it falls
               within the min/max range for the thermal bands. */
            for (i = 0; i < nlines*nsamps; i++)
            {
                /* If this pixel is not fill */
                if (qaband[i] != 1)
                {
                    /* Compute the TOA spectral radiance */
                    tmpf = xcals * uband[i] + xcalo;

                    /* Compute the at-satellite brightness temp (K) and
                       scale for output */
                    tmpf = k2b10 / log (k1b10 / tmpf + 1.0);
                    tmpf = tmpf * MULT_FACTOR_TH;  /* scale the value */

                    /* Make sure the brightness temp falls within the specified
                       range */
                    if (tmpf < MIN_VALID_TH)
                        sband[SR_BAND10][i] = MIN_VALID_TH;
                    else if (tmpf > MAX_VALID_TH)
                        sband[SR_BAND10][i] = MAX_VALID_TH;
                    else
                        sband[SR_BAND10][i] = (int) (tmpf + 0.5);
                }
                else
                    sband[SR_BAND10][i] = FILL_VALUE;
            }
        }  /* end if band 10 */

        else if (ib == DN_BAND11 && strcmp (instrument, "OLI"))
        {
            if (get_input_th_lines (input, 1, 0, nlines, uband) != SUCCESS)
            {
                sprintf (errmsg, "Reading band %d", ib+1);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* Compute brightness temp for band 11.  Make sure it falls
               within the min/max range for the thermal bands. */
            for (i = 0; i < nlines*nsamps; i++)
            {
                /* If this pixel is not fill */
                if (qaband[i] != 1)
                {
                    /* Compute the TOA spectral radiance */
                    tmpf = xcals * uband[i] + xcalo;

                    /* Compute the at-satellite brightness temp (K) and
                       scale for output */
                    tmpf = k2b11 / log (k1b11 / tmpf + 1.0);
                    tmpf = tmpf * MULT_FACTOR_TH;  /* scale the value */

                    /* Make sure the brightness temp falls within the specified
                       range */
                    if (tmpf < MIN_VALID_TH)
                        sband[SR_BAND11][i] = MIN_VALID_TH;
                    else if (tmpf > MAX_VALID_TH)
                        sband[SR_BAND11][i] = MAX_VALID_TH;
                    else
                        sband[SR_BAND11][i] = (int) (tmpf + 0.5);
                }
                else
                    sband[SR_BAND11][i] = FILL_VALUE;
            }
        }  /* end if band 11 */
    }  /* end for ib */
    printf ("\n");

    /* The input data has been read and calibrated. The memory can be freed. */
    free (uband);

    /* Successful completion */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  compute_sr_refl

PURPOSE:  Computes the surfance reflectance for all the reflectance bands.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error computing the reflectance
SUCCESS         No errors encountered

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

HISTORY:
Date          Programmer       Reason
----------    ---------------  -------------------------------------
12/9/2014     Gail Schmidt     Broke the source code into a function to
                               modularize the source code in the main routine

NOTES:
******************************************************************************/
int compute_sr_refl
(
    Input_t *input,     /* I: input structure for the Landsat product */
    Espa_internal_meta_t *xml_metadata,
                        /* I: XML metadata structure */
    char *xml_infile,   /* I: input XML filename */
    uint16 *qaband,     /* I: QA band for the input image, nlines x nsamps */
    int nlines,         /* I: number of lines in reflectance, thermal bands */
    int nsamps,         /* I: number of samps in reflectance, thermal bands */
    float pixsize,      /* I: pixel size for the reflectance bands */
    int16 **sband,      /* I/O: input TOA and output surface reflectance */
    Geoloc_t *space,    /* I: structure for geolocation information */
    Space_def_t *space_def, /* I: structure to define the space mapping */
    float xts,          /* I: solar zenith angle (deg) */
    float xfs,          /* I: solar azimuth angle (deg) */
    float xtv,          /* I: observation zenith angle (deg) */
    float xmus,         /* I: cosine of solar zenith angle */
    float xmuv,         /* I: cosine of observation zenith angle */
    float xfi,          /* I: azimuthal difference between sun and
                              observation (deg) */
    float cosxfi,       /* I: cosine of azimuthal difference */
    float raot550nm,    /* I: nearest value of AOT */
    float pres,         /* I: surface pressure */
    float uoz,          /* I: total column ozone */
    float uwv,          /* I: total column water vapor (precipital water
                              vapor) */
    float **tsmax,      /* I: maximum scattering angle table [20][22] */
    float **tsmin,      /* I: minimum scattering angle table [20][22] */
    float xtsstep,      /* I: solar zenith step value */
    float xtsmin,       /* I: minimum solar zenith value */
    float xtvstep,      /* I: observation step value */
    float xtvmin,       /* I: minimum observation value */
    float tts[22],      /* I: sun angle table */
    float **ttv,        /* I: view angle table [20][22] */
    int32 indts[22],    /* I: index for the sun angle table */
    float ****rolutt,   /* I: intrinsic reflectance table
                              [NSR_BANDS][7][22][8000] */
    float ****transt,   /* I: transmission table [NSR_BANDS][7][22][22] */
    float ***sphalbt,   /* I: spherical albedo table [NSR_BANDS][7][22] */
    float ***normext,   /* I: aerosol extinction coefficient at the current
                              wavelength (normalized at 550nm)
                              [NSR_BANDS][7][22] */
    float **nbfic,      /* I: communitive number of azimuth angles [20][22] */
    float **nbfi,       /* I: number of azimuth angles [20][22] */
    int16 **dem,        /* I: CMG DEM data array [DEM_NBLAT][DEM_NBLON] */
    int16 **andwi,      /* I: avg NDWI [RATIO_NBLAT][RATIO_NBLON] */
    int16 **sndwi,      /* I: standard NDWI [RATIO_NBLAT][RATIO_NBLON] */
    int16 **ratiob1,    /* I: mean band1 ratio [RATIO_NBLAT][RATIO_NBLON] */
    int16 **ratiob2,    /* I: mean band2 ratio [RATIO_NBLAT][RATIO_NBLON] */
    int16 **ratiob7,    /* I: mean band7 ratio [RATIO_NBLAT][RATIO_NBLON] */
    int16 **intratiob1, /* I: integer band1 ratio [RATIO_NBLAT][RATIO_NBLON] */
    int16 **intratiob2, /* I: integer band2 ratio [RATIO_NBLAT][RATIO_NBLON] */
    int16 **intratiob7, /* I: integer band7 ratio [RATIO_NBLAT][RATIO_NBLON] */
    int16 **slpratiob1, /* I: slope band1 ratio [RATIO_NBLAT][RATIO_NBLON] */
    int16 **slpratiob2, /* I: slope band2 ratio [RATIO_NBLAT][RATIO_NBLON] */
    int16 **slpratiob7, /* I: slope band7 ratio [RATIO_NBLAT][RATIO_NBLON] */
    uint16 **wv,        /* I: water vapor values [CMG_NBLAT][CMG_NBLON] */
    uint8 **oz          /* I: ozone values [CMG_NBLAT][CMG_NBLON] */
)
{
    char errmsg[STR_SIZE];                   /* error message */
    char FUNC_NAME[] = "compute_sr_refl";   /* function name */
    int retval;          /* return status */
    int i, j, k, l;      /* looping variable for pixels */
    int ib;              /* looping variable for input bands */
    int iband;           /* current band */
    int curr_pix;        /* current pixel in 1D arrays of nlines * nsamps */
    int win_pix;         /* current pixel in the line,sample window */
    float rotoa;         /* top of atmosphere reflectance */
    float roslamb;       /* lambertian surface reflectance */
    float tgo;           /* other gaseous transmittance */
    float roatm;         /* atmospheric reflectance */
    float ttatmg;
    float satm;          /* spherical albedo */
    float xrorayp;       /* molecular reflectance */
    float next;
    float erelc[NSR_BANDS];    /* band ratio variable for bands 1-7 */
    float troatm[NSR_BANDS];   /* atmospheric reflectance table for bands 1-7 */
    float btgo[NSR_BANDS];     /* other gaseous transmittance for bands 1-7 */
    float broatm[NSR_BANDS];   /* atmospheric reflectance for bands 1-7 */
    float bttatmg[NSR_BANDS];  /* ttatmg for bands 1-7 */
    float bsatm[NSR_BANDS];    /* spherical albedo for bands 1-7 */

    int iband1, iband3; /* band indices (zero-based) */
    float raot;
    float residual;     /* model residual */
    float rsurf;
    float corf;
    long nbclear;                    /* count of the clear (non-cloud) pixels */
    long nbval;                      /* count of the non-fill pixels */
    double anom;                     /* band 3 and 5 combination */
    double mall;                     /* average/mean temp of all the pixels */
    double mclear;                   /* average/mean temp of the clear pixels */
    float fack, facl;                /* cloud height factor in the k,l dim */
    int cldhmin;                     /* minimum bound of the cloud height */
    int cldhmax;                     /* maximum bound of the cloud height */
    float cldh;                      /* cloud height */
    int icldh;                       /* looping variable for cloud height */
    int mband5, mband5k, mband5l;    /* band 6 value and k,l locations */
    float tcloud;                    /* temperature of the current pixel */

    float cfac = 6.0;  /* cloud factor */
    double aaot;
    double sresi;      /* sum of 1 / residuals */
    float fndvi;       /* NDVI value */
    int nbaot;
    int step;
    int hole;
    float ros4, ros5;     /* surface reflectance for band 4 and band 5 */
    int tmp_percent;      /* current percentage for printing status */
    int curr_tmp_percent; /* percentage for current line */

    float lat, lon;       /* pixel lat, long location */
    int lcmg, scmg;       /* line/sample index for the CMG */
    float u, v;           /* line/sample index for the CMG */
    float th1, th2;       /* values for NDWI calculations */
    float xcmg, ycmg;     /* x/y location for CMG */
    float xndwi;          /* calculated NDWI value */
    int uoz11, uoz21, uoz12, uoz22;  /* ozone at line,samp; line, samp+1;
                           line+1, samp; and line+1, samp+1 */
    float pres11, pres12, pres21, pres22;  /* pressure at line,samp;
                           line, samp+1; line+1, samp; and line+1, samp+1 */
    uint8 *cloud = NULL;  /* bit-packed value that represent clouds,
                             nlines x nsamps */
    float *twvi = NULL;   /* interpolated water vapor value,
                             nlines x nsamps */
    float *tozi = NULL;   /* interpolated ozone value, nlines x nsamps */
    float *tp = NULL;     /* interpolated pressure value, nlines x nsamps */
    float *tresi = NULL;  /* residuals for each pixel, nlines x nsamps */
    float *taero = NULL;  /* aerosol values for each pixel, nlines x nsamps */
    int16 *aerob1 = NULL; /* atmospherically corrected band 1 data
                             (TOA refl), nlines x nsamps */
    int16 *aerob2 = NULL; /* atmospherically corrected band 2 data
                             (TOA refl), nlines x nsamps */
    int16 *aerob4 = NULL; /* atmospherically corrected band 4 data
                             (TOA refl), nlines x nsamps */
    int16 *aerob5 = NULL; /* atmospherically corrected band 5 data
                             (TOA refl), nlines x nsamps */
    int16 *aerob7 = NULL; /* atmospherically corrected band 7 data
                             (TOA refl), nlines x nsamps */

    /* Vars for forward/inverse mapping space */
    Img_coord_float_t img;        /* coordinate in line/sample space */
    Geo_coord_t geo;              /* coordinate in lat/long space */

    /* Output file info */
    Output_t *sr_output = NULL;  /* output structure and metadata for the SR
                                    product */
    Envi_header_t envi_hdr;      /* output ENVI header information */
    char envi_file[STR_SIZE];    /* ENVI filename */
    char *cptr = NULL;       /* pointer to the file extension */

    /* Table constants */
    float aot550nm[22] =  /* AOT look-up table */
        {0.01, 0.05, 0.10, 0.15, 0.20, 0.30, 0.40, 0.60, 0.80, 1.00, 1.20,
         1.40, 1.60, 1.80, 2.00, 2.30, 2.60, 3.00, 3.50, 4.00, 4.50, 5.00};
    float tpres[7] =      /* surface pressure table */
        {1050.0, 1013.0, 900.0, 800.0, 700.0, 600.0, 500.0};

    /* Atmospheric correction variables */
    /* Look up table for atmospheric and geometric quantities */
    float tauray[NSR_BANDS] =  /* molecular optical thickness coeff */
        {0.23638, 0.16933, 0.09070, 0.04827, 0.01563, 0.00129, 0.00037,
         0.07984};
    double oztransa[NSR_BANDS] =   /* ozone transmission coeff */
        {-0.00255649, -0.0177861, -0.0969872, -0.0611428, 0.0001, 0.0001,
          0.0001, -0.0834061};
    double wvtransa[NSR_BANDS] =   /* water vapor transmission coeff */
        {2.29849e-27, 2.29849e-27, 0.00194772, 0.00404159, 0.000729136,
         0.00067324, 0.0177533, 0.00279738};
    double wvtransb[NSR_BANDS] =   /* water vapor transmission coeff */
        {0.999742, 0.999742, 0.775024, 0.774482, 0.893085, 0.939669, 0.65094,
         0.759952};
    double ogtransa1[NSR_BANDS] =  /* other gases transmission coeff */
        {4.91586e-20, 4.91586e-20, 4.91586e-20, 1.04801e-05, 1.35216e-05,
         0.0205425, 0.0256526, 0.000214329};
    double ogtransb0[NSR_BANDS] =  /* other gases transmission coeff */
        {0.000197019, 0.000197019, 0.000197019, 0.640215, -0.195998, 0.326577,
         0.243961, 0.396322};
    double ogtransb1[NSR_BANDS] =  /* other gases transmission coeff */
        {9.57011e-16, 9.57011e-16, 9.57011e-16, -0.348785, 0.275239, 0.0117192,
         0.0616101, 0.04728};

    /* Allocate memory for the many arrays needed to do the surface reflectance
       computations */
    retval = memory_allocation_sr (nlines, nsamps, &aerob1, &aerob2, &aerob4,
        &aerob5, &aerob7, &cloud, &twvi, &tozi, &tp, &tresi, &taero);
    if (retval != SUCCESS)
    {   /* get_args already printed the error message */
        sprintf (errmsg, "Error allocating memory for the data arrays needed "
            "for surface reflectance calculations.");
        error_handler (false, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through all the reflectance bands and perform atmospheric
       corrections */
    printf ("Performing atmospheric corrections for each reflectance "
        "band ...");
    for (ib = 0; ib <= SR_BAND7; ib++)
    {
        printf (" %d ...", ib+1);

        /* Get the parameters for the atmospheric correction */
        /* rotoa is not defined for this call, which is ok, but the
           roslamb value is not valid upon output. Just set it to 0.0 to
           be consistent. */
        rotoa = 0.0;
        retval = atmcorlamb2 (xts, xtv, xmus, xmuv, xfi, cosxfi,
            raot550nm, ib, pres, tpres, aot550nm, rolutt, transt, xtsstep,
            xtsmin, xtvstep, xtvmin, sphalbt, normext, tsmax, tsmin, nbfic,
            nbfi, tts, indts, ttv, uoz, uwv, tauray, ogtransa1, ogtransb0,
            ogtransb1, wvtransa, wvtransb, oztransa, rotoa, &roslamb,
            &tgo, &roatm, &ttatmg, &satm, &xrorayp, &next);
        if (retval != SUCCESS)
        {
            sprintf (errmsg, "Performing lambertian atmospheric correction "
                "type 2.");
            error_handler (true, FUNC_NAME, errmsg);
            exit (ERROR);
        }

        /* Save these band-related parameters for later */
        btgo[ib] = tgo;
        broatm[ib] = roatm;
        bttatmg[ib] = ttatmg;
        bsatm[ib] = satm;

        /* Perform atmospheric corrections for bands 1-7 */
        for (i = 0; i < nlines*nsamps; i++)
        {
            /* If this pixel is not fill.  Otherwise fill pixels have
               already been marked in the TOA calculations. */
            if (qaband[i] != 1)
            {
                /* Store the TOA reflectance values, unscaled, for later
                   use before completing atmospheric corrections */
                rotoa = sband[ib][i] * SCALE_FACTOR;
                if (ib == DN_BAND1)
                    aerob1[i] = sband[ib][i];
                else if (ib == DN_BAND2)
                    aerob2[i] = sband[ib][i];
                else if (ib == DN_BAND4)
                    aerob4[i] = sband[ib][i];
                else if (ib == DN_BAND5)
                    aerob5[i] = sband[ib][i];
                else if (ib == DN_BAND7)
                    aerob7[i] = sband[ib][i];

                /* Apply the atmospheric corrections, and store the scaled
                   value for later corrections */
                roslamb = rotoa / tgo;
                roslamb = roslamb - roatm;
                roslamb = roslamb / ttatmg;
                roslamb = roslamb / (1.0 + satm * roslamb);
                sband[ib][i] = (int) (roslamb * MULT_FACTOR);
            }
        }  /* end for i */
    }  /* for ib */
    printf ("\n");

    /* Initialize the band ratios */
    for (ib = 0; ib < NSR_BANDS; ib++)
    {
        erelc[ib] = -1.0;
        troatm[ib] = 0.0;
    }

    /* Interpolate the auxiliary data for each pixel location */
    printf ("Interpolating the auxiliary data ...\n");
    tmp_percent = 0;
    for (i = 0; i < nlines; i++)
    {
        /* update status? */
        curr_tmp_percent = 100 * i / nlines;
        if (curr_tmp_percent > tmp_percent)
        {
            tmp_percent = curr_tmp_percent;
            if (tmp_percent % 10 == 0)
            {
                printf ("%d%% ", tmp_percent);
                fflush (stdout);
            }
        }

        curr_pix = i * nsamps;
        for (j = 0; j < nsamps; j++, curr_pix++)
        {
            /* If this pixel is fill, then don't process */
            if (qaband[curr_pix] == 1)
                continue;

            /* Get the lat/long for the current pixel, for the center of
               the pixel */
            img.l = i - 0.5;
            img.s = j + 0.5;
            img.is_fill = false;
            if (!from_space (space, &img, &geo))
            {
                sprintf (errmsg, "Mapping line/sample (%d, %d) to "
                    "geolocation coords", i, j);
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }
            lat = geo.lat * RAD2DEG;
            lon = geo.lon * RAD2DEG;

            /* Use that lat/long to determine the line/sample in the
               CMG-related lookup tables, using the center of the UL
               pixel */
            ycmg = (89.975 - lat) * 20.0;   /* vs / 0.05 */
            xcmg = (179.975 + lon) * 20.0;  /* vs / 0.05 */
            lcmg = (int) (ycmg);
            scmg = (int) (xcmg);
            if ((lcmg < 0 || lcmg >= CMG_NBLAT) ||
                (scmg < 0 || scmg >= CMG_NBLON))
            {
                sprintf (errmsg, "Invalid line/sample combination for the "
                    "CMG-related lookup tables - line %d, sample %d "
                    "(0-based). CMG-based tables are %d lines x %d "
                    "samples.", lcmg, scmg, CMG_NBLAT, CMG_NBLON);
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }

            u = (ycmg - lcmg);
            v = (xcmg - scmg);
            twvi[curr_pix] = wv[lcmg][scmg] * (1.0 - u) * (1.0 - v) +
                             wv[lcmg][scmg+1] * (1.0 - u) * v +
                             wv[lcmg+1][scmg] * u * (1.0 - v) +
                             wv[lcmg+1][scmg+1] * u * v;
            twvi[curr_pix] = twvi[curr_pix] * 0.01;   /* vs / 100 */

            uoz11 = oz[lcmg][scmg];
            if (uoz11 == 0)
                uoz11 = 120;

            uoz12 = oz[lcmg][scmg+1];
            if (uoz12 == 0)
                uoz12 = 120;

            uoz21 = oz[lcmg+1][scmg];
            if (uoz21 == 0)
                uoz21 = 120;

            uoz22 = oz[lcmg+1][scmg+1];
            if (uoz22 == 0)
                uoz22 = 120;

            tozi[curr_pix] = uoz11 * (1.0 - u) * (1.0 - v) +
                             uoz12 * (1.0 - u) * v +
                             uoz21 * u * (1.0 - v) +
                             uoz22 * u * v;
            tozi[curr_pix] = tozi[curr_pix] * 0.0025;   /* vs / 400 */

            if (dem[lcmg][scmg] != -9999)
                pres11 = 1013.0 * exp (-dem[lcmg][scmg] * ONE_DIV_8500);
            else
            {
                pres11 = 1013.0;
                cloud[curr_pix] = 128;    /* set water bit */
                tresi[curr_pix] = -1.0;
            }

            if (dem[lcmg][scmg+1] != -9999)
                pres12 = 1013.0 * exp (-dem[lcmg][scmg+1] * ONE_DIV_8500);
            else
                pres12 = 1013.0;

            if (dem[lcmg+1][scmg] != -9999)
                pres21 = 1013.0 * exp (-dem[lcmg+1][scmg] * ONE_DIV_8500);
            else
                pres21 = 1013.0;

            if (dem[lcmg+1][scmg+1] != -9999)
                pres22 = 1013.0 * exp (-dem[lcmg+1][scmg+1] * ONE_DIV_8500);
            else
                pres22 = 1013.0;

            tp[curr_pix] = pres11 * (1.0 - u) * (1.0 - v) +
                           pres12 * (1.0 - u) * v +
                           pres21 * u * (1.0 - v) +
                           pres22 * u * v;

            /* Inverting aerosols */
            /* Filter cirrus pixels */
            if (sband[SR_BAND9][curr_pix] >
                (100.0 / (tp[curr_pix] * ONE_DIV_1013)))
            {  /* Set cirrus bit */
                cloud[curr_pix]++;
            }
            else
            {  /* Inverting aerosol */
                if (ratiob1[lcmg][scmg] == 0)
                {
                    /* Average the valid ratio around the location */
                    erelc[DN_BAND1] = 0.4817;
                    erelc[DN_BAND2] = erelc[DN_BAND1] / 0.844239;
                    erelc[DN_BAND4] = 1.0;
                    erelc[DN_BAND7] = 1.79;
                }
                else
                {
                    /* Use the NDWI to calculate the band ratio */
                    xndwi = ((double) sband[SR_BAND5][curr_pix] -
                             (double) (sband[SR_BAND7][curr_pix] * 0.5)) /
                            ((double) sband[SR_BAND5][curr_pix] +
                             (double) (sband[SR_BAND7][curr_pix] * 0.5));

                    th1 = (andwi[lcmg][scmg] + 2.0 * sndwi[lcmg][scmg]) *
                        0.001;
                    th2 = (andwi[lcmg][scmg] - 2.0 * sndwi[lcmg][scmg]) *
                        0.001;
                    if (xndwi > th1)
                        xndwi = th1;
                    if (xndwi < th2)
                        xndwi = th2;

                    erelc[DN_BAND1] = (xndwi * slpratiob1[lcmg][scmg] +
                        intratiob1[lcmg][scmg]) * 0.001;
                    erelc[DN_BAND2] = (xndwi * slpratiob2[lcmg][scmg] +
                        intratiob2[lcmg][scmg]) * 0.001;
                    erelc[DN_BAND4] = 1.0;
                    erelc[DN_BAND7] = (xndwi * slpratiob7[lcmg][scmg] +
                        intratiob7[lcmg][scmg]) * 0.001;
                }

                troatm[DN_BAND1] = aerob1[curr_pix] * SCALE_FACTOR;
                troatm[DN_BAND2] = aerob2[curr_pix] * SCALE_FACTOR;
                troatm[DN_BAND4] = aerob4[curr_pix] * SCALE_FACTOR;
                troatm[DN_BAND7] = aerob7[curr_pix] * SCALE_FACTOR;

                /* If this is water ... */
                if (btest (cloud[curr_pix], WAT_QA))
                {
                    /* Check the NDVI */
                    fndvi = ((double) sband[SR_BAND5][curr_pix] -
                             (double) sband[SR_BAND4][curr_pix]) /
                            ((double) sband[SR_BAND5][curr_pix] +
                             (double) sband[SR_BAND4][curr_pix]);
                    if (fndvi < 0.1)
                    {  /* skip the rest of the processing */
                        taero[curr_pix] = 0.0;
                        tresi[curr_pix] = -0.01;
                        continue;
                    }
                }
       
                iband1 = DN_BAND4;
                iband3 = DN_BAND1;
                retval = subaeroret (iband1, iband3, xts, xtv, xmus, xmuv,
                    xfi, cosxfi, pres, uoz, uwv, erelc, troatm, tpres,
                    aot550nm, rolutt, transt, xtsstep, xtsmin, xtvstep,
                    xtvmin, sphalbt, normext, tsmax, tsmin, nbfic, nbfi,
                    tts, indts, ttv, tauray, ogtransa1, ogtransb0,
                    ogtransb1, wvtransa, wvtransb, oztransa, &raot,
                    &residual, &next);
                if (retval != SUCCESS)
                {
                    sprintf (errmsg, "Performing atmospheric correction.");
                    error_handler (true, FUNC_NAME, errmsg);
                    exit (ERROR);
                }
                corf = raot / xmus;

                if (residual < (0.015 + 0.005 * corf))
                {  /* test if band 5 makes sense */
                    iband = DN_BAND5;
                    rotoa = aerob5[curr_pix] * SCALE_FACTOR;
                    raot550nm = raot;
                    retval = atmcorlamb2 (xts, xtv, xmus, xmuv, xfi, cosxfi,
                        raot550nm, iband, pres, tpres, aot550nm, rolutt,
                        transt, xtsstep, xtsmin, xtvstep, xtvmin, sphalbt,
                        normext, tsmax, tsmin, nbfic, nbfi, tts, indts,
                        ttv, uoz, uwv, tauray, ogtransa1, ogtransb0,
                        ogtransb1, wvtransa, wvtransb, oztransa, rotoa,
                        &roslamb, &tgo, &roatm, &ttatmg, &satm, &xrorayp,
                        &next);
                    if (retval != SUCCESS)
                    {
                        sprintf (errmsg, "Performing lambertian "
                            "atmospheric correction type 2.");
                        error_handler (true, FUNC_NAME, errmsg);
                        exit (ERROR);
                    }
                    ros5 = roslamb;

                    iband = DN_BAND4;
                    rotoa = aerob4[curr_pix] * SCALE_FACTOR;
                    raot550nm = raot;
                    retval = atmcorlamb2 (xts, xtv, xmus, xmuv, xfi, cosxfi,
                        raot550nm, iband, pres, tpres, aot550nm, rolutt,
                        transt, xtsstep, xtsmin, xtvstep, xtvmin, sphalbt,
                        normext, tsmax, tsmin, nbfic, nbfi, tts, indts,
                        ttv, uoz, uwv, tauray, ogtransa1, ogtransb0,
                        ogtransb1, wvtransa, wvtransb, oztransa, rotoa,
                        &roslamb, &tgo, &roatm, &ttatmg, &satm, &xrorayp,
                        &next);
                    if (retval != SUCCESS)
                    {
                        sprintf (errmsg, "Performing lambertian "
                            "atmospheric correction type 2.");
                        error_handler (true, FUNC_NAME, errmsg);
                        exit (ERROR);
                    }
                    ros4 = roslamb;

                    if ((ros5 > 0.1) && ((ros5 - ros4) / (ros5 + ros4) > 0))
                    {
                        taero[curr_pix] = raot;
                        tresi[curr_pix] = residual;
                    }
                    else
                    {
                        taero[curr_pix] = 0.0;
                        tresi[curr_pix] = -0.01;
                    }
                }
                else
                {
                    taero[curr_pix] = 0.0;
                    tresi[curr_pix] = -0.01;
                }
            }  /* end if cirrus */
        }  /* end for i */
    }  /* end for j */

    /* update status */
    printf ("100%%\n");
    fflush (stdout);

    /* Done with the aerob* arrays */
    free (aerob1);  aerob1 = NULL;
    free (aerob2);  aerob2 = NULL;
    free (aerob4);  aerob4 = NULL;
    free (aerob5);  aerob5 = NULL;
    free (aerob7);  aerob7 = NULL;

    /* Refine the cloud mask */
    /* Compute the average temperature of the clear, non-water, non-filled
       pixels */
    printf ("Refining the cloud mask ...\n");
    nbval = 0;
    nbclear = 0;
    mclear = 0.0;
    mall = 0.0;
    for (i = 0; i < nlines*nsamps; i++)
    {
        /* If this pixel is fill, then don't process */
        if (qaband[i] != 1)
        {
            nbval++;
            mall += sband[SR_BAND10][i] * SCALE_FACTOR_TH;
            if ((!btest (cloud[i], CIR_QA)) &&
                (sband[SR_BAND5][i] > 300))
            {
                anom = sband[SR_BAND2][i] - sband[SR_BAND4][i] * 0.5;
                if (anom < 300)
                {
                    nbclear++;
                    mclear += sband[SR_BAND10][i] * SCALE_FACTOR_TH;
                }
            }
        }
    }  /* end for i */

    if (nbclear > 0)
        mclear = mclear / nbclear;
    else
        mclear = 275.0;

    if (nbval > 0)
        mall = mall / nbval;

    printf ("Average clear temperature %%clear %f %f %f %ld\n", mclear,
        nbclear * 100.0 / (nlines * nsamps), mall, nbval);

    /* Determine the cloud mask */
    for (i = 0; i < nlines*nsamps; i++)
    {
        if (tresi[i] < 0.0)
        {
            if (((sband[SR_BAND2][i] - sband[SR_BAND4][i] * 0.5) > 500) &&
                ((sband[SR_BAND10][i] * SCALE_FACTOR_TH) < (mclear - 2.0)))
            {  /* Snow or cloud for now */
                cloud[i] += 2;
            }
        }
    }

    /* Set up the adjacent to something bad (snow or cloud) bit */
    printf ("Setting up the adjacent to something bit ...\n");
    for (i = 0; i < nlines; i++)
    {
        curr_pix = i * nsamps;
        for (j = 0; j < nsamps; j++, curr_pix++)
        {
            if (btest (cloud[curr_pix], CLD_QA) ||
                btest (cloud[curr_pix], CIR_QA))
            {
                /* Check the 5x5 window around the current pixel */
                for (k = i-5; k <= i+5; k++)
                {
                    /* Make sure the line is valid */
                    if (k < 0 || k >= nlines)
                        continue;

                    win_pix = k * nsamps + j-5;
                    for (l = j-5; l <= j+5; l++, win_pix++)
                    {
                        /* Make sure the sample is valid */
                        if (l < 0 || l >= nsamps)
                            continue;

                        if (!btest (cloud[win_pix], CLD_QA) &&
                            !btest (cloud[win_pix], CIR_QA) &&
                            !btest (cloud[win_pix], CLDA_QA))
                        {  /* Set the adjacent cloud bit */
                            cloud[win_pix] += 4;
                        }
                    }  /* for l */
                }  /* for k */
            }  /* if btest */
        }  /* for j */
    }  /* for i */

    /* Compute the cloud shadow */
    printf ("Determining cloud shadow ...\n");
    facl = cosf(xfs * DEG2RAD) * tanf(xts * DEG2RAD) / pixsize;  /* lines */
    fack = sinf(xfs * DEG2RAD) * tanf(xts * DEG2RAD) / pixsize;  /* samps */
    for (i = 0; i < nlines; i++)
    {
        curr_pix = i * nsamps;
        for (j = 0; j < nsamps; j++, curr_pix++)
        {
            if (btest (cloud[curr_pix], CLD_QA) ||
                btest (cloud[curr_pix], CIR_QA))
            {
                tcloud = sband[SR_BAND10][curr_pix] * SCALE_FACTOR_TH;
                cldh = (mclear - tcloud) * 1000.0 / cfac;
                if (cldh < 0.0)
                    cldh = 0.0;
                cldhmin = cldh - 1000.0;
                cldhmax = cldh + 1000.0;
                mband5 = 9999;
                mband5k = -9999;
                mband5l = -9999;
                if (cldhmin < 0)
                    cldhmin = 0.0;
                for (icldh = cldhmin * 0.1; icldh <= cldhmax * 0.1; icldh++)
                {
                    cldh = icldh * 10.0;
                    k = i + facl * cldh;  /* lines */
                    l = j - fack * cldh;  /* samps */
                    /* Make sure the line and sample is valid */
                    if (k < 0 || k >= nlines || l < 0 || l >= nsamps)
                        continue;

                    win_pix = k * nsamps + l;
                    if ((sband[SR_BAND6][win_pix] < 800) &&
                        ((sband[SR_BAND3][win_pix] -
                          sband[SR_BAND4][win_pix]) < 100))
                    {
                        if (btest (cloud[win_pix], CLD_QA) ||
                            btest (cloud[win_pix], CIR_QA) ||
                            btest (cloud[win_pix], CLDS_QA))
                        {
                            continue;
                        }
                        else
                        { /* store the value of band6 as well as the
                             l and k value */
                            if (sband[SR_BAND6][win_pix] < mband5)
                            {
                                 mband5 = sband[SR_BAND6][win_pix];
                                 mband5k = k;
                                 mband5l = l;
                            }
                        }
                    }
                }  /* for icldh */

                /* Set the cloud shadow bit */
                if (mband5 < 9999)
                    cloud[mband5k*nsamps + mband5l] += 8;
            }  /* end if btest */
        }  /* end for j */
    }  /* end for i */

    /* Expand the cloud shadow using the residual */
    printf ("Expanding cloud shadow ...\n");
    for (i = 0; i < nlines; i++)
    {
        curr_pix = i * nsamps;
        for (j = 0; j < nsamps; j++, curr_pix++)
        {
            /* If this is a cloud shadow pixel */
            if (btest (cloud[curr_pix], CLDS_QA))
            {
                /* Check the 6x6 window around the current pixel */
                for (k = i-6; k <= i+6; k++)
                {
                    /* Make sure the line is valid */
                    if (k < 0 || k >= nlines)
                        continue;

                    win_pix = k * nsamps + j-6;
                    for (l = j-6; l <= j+6; l++, win_pix++)
                    {
                        /* Make sure the sample is valid */
                        if (l < 0 || l >= nsamps)
                            continue;

                        if (btest (cloud[win_pix], CLD_QA) ||
                            btest (cloud[win_pix], CLDS_QA))
                            continue;
                        else
                        {
                            if (btest (cloud[win_pix], CLDT_QA))
                                continue;
                            else
                            {
                                /* Set the temporary bit */
                                if (tresi[win_pix] < 0)
                                    cloud[win_pix] += 16;
                            }
                        }
                    }  /* end for l */
                }  /* end for k */
            }  /* end if btest */
        }  /* end for j */
    }  /* end for i */

    /* Update the cloud shadow */
    printf ("Updating cloud shadow ...\n");
    for (i = 0; i < nlines*nsamps; i++)
    {
        /* If the temporary bit was set in the above loop */
        if (btest (cloud[i], CLDT_QA))
        {
            /* Remove the temporary bit and set the cloud shadow bit */
            /* ==> cloud[i] += 8; cloud[i] -= 16; */
            cloud[i] -= 8;
        }
    }  /* end for i */

    /* Aerosol interpolation */
    printf ("Performing aerosol interpolation ...\n");
    hole = 1;
    step = 10;
    while ((hole != 0) && (step < 1000))
    {
        hole = 0;
        for (i = 0; i < nlines; i += step)
        {
            for (j = 0; j < nsamps; j += step)
            {
                nbaot = 0;
                aaot = 0.0;
                sresi = 0.0;

                /* Check the window around the current pixel */
                for (k = i; k <= i+step-1; k++)
                {
                    /* Make sure the line is valid */
                    if (k < 0 || k >= nlines)
                        continue;

                    win_pix = k * nsamps + j;
                    for (l = j; l <= j+step-1; l++, win_pix++)
                    {
                        /* Make sure the sample is valid */
                        if (l < 0 || l >= nsamps)
                            continue;

                        if ((tresi[win_pix] > 0) && (cloud[win_pix] == 0))
                        {
                            nbaot++;
                            aaot += taero[win_pix] / tresi[win_pix];
                            sresi += 1.0 / tresi[win_pix];
                        }
                    }
                }

                /* If pixels were found */
                if (nbaot != 0)
                {
                    aaot /= sresi;

                    /* Check the window around the current pixel */
                    for (k = i; k <= i+step-1; k++)
                    {
                        /* Make sure the line is valid */
                        if (k < 0 || k >= nlines)
                            continue;

                        win_pix = k * nsamps + j;
                        for (l = j; l <= j+step-1; l++, win_pix++)
                        {
                            /* Make sure the sample is valid */
                            if (l < 0 || l >= nsamps)
                                continue;

                            if ((tresi[win_pix] < 0) &&
                                (!btest (cloud[win_pix], CIR_QA)) &&
                                (!btest (cloud[win_pix], CLD_QA)) &&
                                (!btest (cloud[win_pix], WAT_QA)))
                            {
                                taero[win_pix] = aaot;
                                tresi[win_pix] = 1.0;
                            }
                        }  /* for l */
                    }  /* for k */
                }
                else
                {  /* this is a hole */
                    hole++;
                }
            }  /* end for j */
        }  /* end for i */

        /* Modify the step value */
        step *= 2;
    }  /* end while */

    /* Perform the atmospheric correction */
    printf ("Performing atmospheric correction ...\n");
    /* 0 .. DN_BAND7 is the same as 0 .. SR_BAND7 here, since the pan band
       isn't spanned */
    for (ib = 0; ib <= DN_BAND7; ib++)
    {
        printf ("  Band %d\n", ib+1);
        for (i = 0; i < nlines * nsamps; i++)
        {
            /* If this pixel is fill, then don't process. Otherwise the
               fill pixels have already been marked in the TOA process. */
            if (qaband[i] != 1)
            {
                if (tresi[i] > 0.0 &&
                    !btest (cloud[i], CIR_QA) &&
                    !btest (cloud[i], CLD_QA))
                {
                    rsurf = sband[ib][i] * SCALE_FACTOR;
                    rotoa = (rsurf * bttatmg[ib] / (1.0 - bsatm[ib] * rsurf)
                        + broatm[ib]) * btgo[ib];
                    raot550nm = taero[i];
                    pres = tp[i];
                    uwv = twvi[i];
                    uoz = tozi[i];
                    retval = atmcorlamb2 (xts, xtv, xmus, xmuv, xfi, cosxfi,
                        raot550nm, ib, pres, tpres, aot550nm, rolutt,
                        transt, xtsstep, xtsmin, xtvstep, xtvmin, sphalbt,
                        normext, tsmax, tsmin, nbfic, nbfi, tts, indts,
                        ttv, uoz, uwv, tauray, ogtransa1, ogtransb0,
                        ogtransb1, wvtransa, wvtransb, oztransa, rotoa,
                        &roslamb, &tgo, &roatm, &ttatmg, &satm, &xrorayp,
                        &next);
                    if (retval != SUCCESS)
                    {
                        sprintf (errmsg, "Performing lambertian "
                            "atmospheric correction type 2.");
                        error_handler (true, FUNC_NAME, errmsg);
                        exit (ERROR);
                    }

                    /* Handle the aerosol computation in the cloud mask if
                       this is the cirrus band */
                    if (ib == DN_BAND1)
                    {
                        if (roslamb < -0.005)
                        {
                            taero[i] = 0.05;
                            raot550nm = 0.05;
                            retval = atmcorlamb2 (xts, xtv, xmus, xmuv,
                                xfi, cosxfi, raot550nm, ib, pres, tpres,
                                aot550nm, rolutt, transt, xtsstep, xtsmin,
                                xtvstep, xtvmin, sphalbt, normext, tsmax,
                                tsmin, nbfic, nbfi, tts, indts, ttv, uoz,
                                uwv, tauray, ogtransa1, ogtransb0,
                                ogtransb1, wvtransa, wvtransb, oztransa,
                                rotoa, &roslamb, &tgo, &roatm, &ttatmg,
                                &satm, &xrorayp, &next);
                            if (retval != SUCCESS)
                            {
                                sprintf (errmsg, "Performing lambertian "
                                    "atmospheric correction type 2.");
                                error_handler (true, FUNC_NAME, errmsg);
                                exit (ERROR);
                            }
                        }
                        else
                        {  /* Set up aerosol QA bits */
                            if (fabs (rsurf - roslamb) <= 0.015)
                            {  /* Set the first aerosol bit */
                                cloud[i] += 16;
                            }
                            else
                            {
                                if (fabs (rsurf - roslamb) < 0.03)
                                {  /* Set the second aerosol bit */
                                    cloud[i] += 32;
                                }
                                else
                                {  /* Set both aerosol bits */
                                    cloud[i] += 48;
                                }
                            }
                        }  /* end if/else roslamb */
                    }  /* end if ib */

                    /* Save the scaled surface reflectance value, but make
                       sure it falls within the defined valid range. */
                    roslamb = roslamb * MULT_FACTOR;  /* scale the value */
                    if (roslamb < MIN_VALID)
                        sband[ib][i] = MIN_VALID;
                    else if (roslamb > MAX_VALID)
                        sband[ib][i] = MAX_VALID;
                    else
                        sband[ib][i] = (int) roslamb;
                }  /* end if */
            }  /* end if qaband */
        }  /* end for i */
    }  /* end for ib */

    /* Free memory for band data */
    free (twvi);
    free (tozi);
    free (tp);
    free (tresi);
    free (taero);
 
    /* Write the data to the output file */
    printf ("Writing surface reflectance corrected data to the output "
        "files ...\n");

    /* Open the output file */
    sr_output = open_output (xml_metadata, input, false /*surf refl*/);
    if (sr_output == NULL)
    {   /* error message already printed */
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Loop through the reflectance bands and write the data */
    for (ib = 0; ib <= DN_BAND7; ib++)
    {
        printf ("  Band %d: %s\n", ib+1,
            sr_output->metadata.band[ib].file_name);
        if (put_output_lines (sr_output, sband[ib], ib, 0, nlines,
            sizeof (int16)) != SUCCESS)
        {
            sprintf (errmsg, "Writing output data for band %d", ib);
            error_handler (true, FUNC_NAME, errmsg);
            exit (ERROR);
        }

        /* Create the ENVI header file this band */
        if (create_envi_struct (&sr_output->metadata.band[ib],
            &xml_metadata->global, &envi_hdr) != SUCCESS)
        {
            sprintf (errmsg, "Creating ENVI header structure.");
            error_handler (true, FUNC_NAME, errmsg);
            exit (ERROR);
        }

        /* Write the ENVI header */
        strcpy (envi_file, sr_output->metadata.band[ib].file_name);
        cptr = strchr (envi_file, '.');
        strcpy (cptr, ".hdr");
        if (write_envi_hdr (envi_file, &envi_hdr) != SUCCESS)
        {
            sprintf (errmsg, "Writing ENVI header file.");
            error_handler (true, FUNC_NAME, errmsg);
            exit (ERROR);
        }
    }

    /* Append the surface reflectance bands (1-7) to the XML file */
    if (append_metadata (7, sr_output->metadata.band, xml_infile) !=
        SUCCESS)
    {
        sprintf (errmsg, "Appending surface reflectance bands to the "
            "XML file.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Write the cloud mask band */
    printf ("  Band %d: %s\n", SR_CLOUD+1,
            sr_output->metadata.band[SR_CLOUD].file_name);
    if (put_output_lines (sr_output, cloud, SR_CLOUD, 0, nlines,
        sizeof (uint8)) != SUCCESS)
    {
        sprintf (errmsg, "Writing cloud mask output data");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Free memory for cloud data */
    free (cloud);

    /* Create the ENVI header for the cloud mask band */
    if (create_envi_struct (&sr_output->metadata.band[SR_CLOUD],
        &xml_metadata->global, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Creating ENVI header structure.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Write the ENVI header */
    strcpy (envi_file, sr_output->metadata.band[SR_CLOUD].file_name);
    cptr = strchr (envi_file, '.');
    strcpy (cptr, ".hdr");
    if (write_envi_hdr (envi_file, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Writing ENVI header file.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Append the cloud mask band to the XML file */
    if (append_metadata (1, &sr_output->metadata.band[SR_CLOUD],
        xml_infile) != SUCCESS)
    {
        sprintf (errmsg, "Appending cloud mask band to XML file.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Close the output surface reflectance products */
    close_output (sr_output, false /*sr products*/);
    free_output (sr_output);

    /* Successful completion */
    return (SUCCESS);
}

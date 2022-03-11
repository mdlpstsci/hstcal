# include <stdio.h>

# include "hstcal.h"
# include "hstio.h"
# include "wf3.h"
# include "wf3dq.h"
# include "wf3info.h"
# include "hstcalerr.h"

/*
  This routine compares the full-well saturation image pixels against
  those of the SingleGroup x science array pixels.  If the science array
  pixel value >= the value in the saturation image, the corresponding
  SingleGroup x dq array pixel is ORed with the full-well saturation
  flag, SATPIXEL, and the dq array is modified in-place.
  The WFC3 science data must have already been corrected for bias (BIASCORR)
  and bias level (BLEVCORR).
  Note the saturation image data is only valid in the region of a
  trimmed full-frame image.  In order to apply the saturation image to the
  bias/blev corrected science data (full-frame or subarray), the starting
  X and Y pixels, as well as the X and Y sizes need to be obtained, and
  the saturation image properly overlaid on the science data.
  Note that formerly the full-well saturation flags were applied during
  doDQI.

  Michele De La Pena, 2020 May 14
  Initial implementation of the full-well saturation flagging done
  via an image.
  Michele De La Pena, 2021 Feb 03
  Read entire saturation image into memory rather than line by line access.
 */

int doFullWellSat(WF3Info *wf3, SingleGroup *x) {

    /*
      WF3Info *wf3     i: calibration switches, etc.
      SingleGroup *x   io: image to be modified; dq array is updated in-place
     */

    extern int status;

    SingleGroupLine y, z;	/* y and z are scratch space */
    SingleGroup satimage;   /* storage for entire saturation image */
    int extver = 1;			/* get this imset from bias image */
    int rx, ry;				/* for binning bias image down to size of x */
    int x0, y0;				/* offsets of sci image */
    int same_size;			/* true if no binning of ref image required */
    int avg = 0;			/* bin2d should sum within each bin */
    int xdim;
    int ydim;				/* number of lines in science image */
    int i, j, k;
	short sum_dq;
    int xbeg, ybeg;			/* Beginning pixels for saturation image overlay */
    int xend, yend;			/* Beginning pixels for saturation image overlay */
    int rsize = 1;
    int sci_bin[2];
    int sci_corner[2];
    int ref_bin[2];
    int ref_corner[2];

    int FindLine (SingleGroup *, SingleGroupLine *, int *, int *,int *, int *, int *);
    int trim1d (SingleGroupLine *, int, int, int, int, int, SingleGroupLine *);
    int DetCCDChip (char *, int, int, int *);
	int GetCorner (Hdr *, int, int *, int *);

    /* Initialize local variables */
    rx = 1;
    ry = 1;
    x0 = 0;
    y0 = 0;
    same_size = 1;

    /* 
       Compute correct extension version number to extract from
       reference image to correspond to CHIP in science data.
       Note: wf3->nimsets is no longer used in DetCCDChip() routine.
    */
    if (DetCCDChip (wf3->satmap.name, wf3->chip, wf3->nimsets, &extver) )
        return (status);

    /* Get the first line of saturation image data */
    initSingleGroupLine (&y);
    openSingleGroupLine (wf3->satmap.name, extver, &y);
    if (hstio_err())
        return (status = OPEN_FAILED);

    /* 
       Reference image should already be selected to have the
       same binning factor as the science image.  All we need to
       make sure of is whether the science array is a sub-array of
       the bias image.  

       x0,y0 is the location of the start of the 
       subimage in the reference image.
     */
    if (FindLine (x, &y, &same_size, &rx, &ry, &x0, &y0))
        return (status);
     sprintf (MsgText, "*********Img info. same: %d rx: %d ry: %d x0: %d y0:%d", same_size, rx, ry, x0, y0);
     trlmessage(MsgText);

    /* Clean up the SingleGroupLine object */
    closeSingleGroupLine (&y);
    freeSingleGroupLine (&y);

    if (wf3->verbose) {
        sprintf (MsgText, "Ratio of (%d,%d) with offset =(%d,%d)",
                rx, ry, x0, y0);
        trlmessage(MsgText);
        if (same_size) {
            sprintf(MsgText,"Saturation image and input are the same size ");
        } else {
            sprintf(MsgText,"Saturation image and input are NOT the same size ");
        }
        trlmessage(MsgText);
    }

    /* If reference data not binned same as input, return with an error */
    if (rx != 1 || ry != 1) {
        sprintf (MsgText,
                "Saturation image and input are not binned to the same pixel size!");
        trlerror (MsgText);
        return (status = SIZE_MISMATCH);
    }

	if (wf3->verbose){
	    sprintf(MsgText,"Image has starting location of %d,%d in the reference image",x0,y0);
	    trlmessage(MsgText);
	}

    /* Get the full saturation image */
    initSingleGroup(&satimage);
    getSingleGroup(wf3->satmap.name, extver, &satimage);
    if (hstio_err()) {
        freeSingleGroup (&satimage);
        return (status = OPEN_FAILED);
    }

    /* If the science and reference images are not the same size, check to
       see if we're calibrating a subarray image with a full-frame, 4-amp
       bias image. The full-frame bias will contain serial virtual overscan
       columns in the middle of it, which must be accounted for in the value
       of x0 if the science image subarray is located to the right (higher
       column numbers) of the virtual overscan, which will be the case for
       science image subarrays located in the amp B and D quadrants.
       FIX: MDD
     */
/*
    if (!same_size) {
        sprintf(MsgText,"Bias ref and science image not the same size");
        trlmessage(MsgText);
*/
        /* Retrieve CCDAMP value from bias image header */
/*
        if(GetKeyStr(y.globalhdr, "CCDAMP", NO_DEFAULT, "", biasamp, SZ_CBUF))
            return (status);

*/
        /* If the bias image uses all 4 amps and the science image uses
           either amp B or D, then add the virtual overscan columns to x0 */
/*
        if ((streq_ic(biasamp,"ABCD") || streq_ic(biasamp,"ANY") \
                    || streq_ic(biasamp,"SINGLE_AMP") ||streq_ic(biasamp,"SINGLE_OR_ALL") ) &&
                (streq_ic(wf3->ccdamp,"B") || streq_ic(wf3->ccdamp,"D"))) {
            x0 += 60;

        }
    }
*/

    /* Compute the output size */
    xdim = x->sci.data.nx - (wf3->trimx[0] + wf3->trimx[1]);
    ydim = x->sci.data.ny - (wf3->trimy[0] + wf3->trimy[1]);
    //xdim = x->sci.data.nx;
    //ydim = x->sci.data.ny;

    if (same_size) {
        sprintf(MsgText, "Full-frame full-well saturation image flagging step being performed.\n");
        trlmessage(MsgText);

        /* Define the beginning and ending pixels */
        xbeg = abs(sci_corner[0]);	
        ybeg = abs(sci_corner[1]);
        xend = xbeg + xdim;
        yend = ybeg + ydim;

        /* Loop over the lines in the science image, excluding the overscan lines */
        {unsigned int  j;
        for (j = ybeg; j < yend; j++) {
            {unsigned int  i;
            for (i = xbeg;  i < xend;  i++) {

                /* Flag full-well saturated pixels with 256 dq bit*/             
		        if (Pix (x->sci.data, i, j) > Pix(satimage.sci.data, i, j)) {
			        sum_dq = DQPix (x->dq.data, i, j) | SATPIXEL;
			        DQSetPix (x->dq.data, i, j, sum_dq);
                }
            }}
        }}
        sprintf(MsgText, "Full-frame full-well saturation image flagging step done.\n");
        trlmessage(MsgText);
    } else {  /* subarray */
        sprintf(MsgText, "Subarray full-well saturation image flagging step being performed.\n");
        trlmessage(MsgText);

        /* Compute the end point of the X and Y dimension loops */
        xend = x0 + xdim;
        yend = y0 + ydim;

        /* Loop over all the lines in the science array, and
           match them to the appropriate line in the reference image...

           j - index for line in science image
           k - index for line in reference image
           y0 - line in reference image corresponding to line in input image
        */
        {unsigned int j, k;
        for (j = 0, k = y0; j < ydim; j++, k++) {

            /* 
               Working with a sub-array so need to apply the proper 
               section from the reference image to the science image.
            */

            {unsigned int i, l;
            for (i = 0, l = x0; i < xdim; i++, l++) {
                /* Flag full-well saturated pixels with 256 dq bit*/             
		        if (Pix (x->sci.data, i, j) > Pix(satimage.sci.data, l, k)) {
			        sum_dq = DQPix (x->dq.data, i, j) | SATPIXEL;
			        DQSetPix (x->dq.data, i, j, sum_dq);
		        }
            }}
        }}
        sprintf(MsgText, "Subarray full-well saturation image flagging step done.\n");
        trlmessage(MsgText);
    }

    freeSingleGroup (&satimage);

    return (status);
}

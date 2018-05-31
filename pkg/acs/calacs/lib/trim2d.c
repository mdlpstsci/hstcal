# include <stdlib.h>		/* calloc */
# include <string.h>		/* strncmp */
# include <math.h>		/* sqrt */
#include "hstcal.h"
# include "hstio.h"
# include "hstcalerr.h"	/* SIZE_MISMATCH */
# include "acs.h"

/* This routine is a mild modification of the trim1d routine from trim.c. This trim2d routine 
   takes an input data image, extracts a specified subset, and assigns the values to 
   an output data array.  The calling routine must allocate the output SingleGroup (setting 
   its size) and free it when done.  If requested, the coordinate keywords in the output 
   extension headers will be updated.
   M.D. De La Pena: 05 June 2018

   This function does not treat any expansion or reduction in Y!
*/

int trim2d (SingleGroup *a, int xstart, int ystart, int binx, int biny, int update, SingleGroup *b) {

/* arguments:
SingleGroup *a        i: input data
int xstart  		  i: starting location of subarray (zero indexed,
						 input pixel coordinates)
int ystart  		  i: starting location of subarray (zero indexed,
						 input pixel coordinates)
int binx	          i: number of X input pixels for one output pixel
int biny	          i: number of Y input pixels for one output pixel
int update			  i: == 1 YES, update header info of output data
						 == 0 NO, do NOT update header info of output data
SingleGroup *b        o: output data
*/

	extern int status;

	double block[2];	/* number of input pixels for one output */
	double offset[2];	/* offset of binned image */
	int i, j;           /* pixel indices in input array */
	int m, n;           /* pixel index in output array */

	int BinCoords (Hdr *, double *, double *, Hdr *, Hdr *, Hdr *);

	/* Initialize internal variables... */
	int nx = b->sci.data.tot_nx;
    int ny = b->sci.data.tot_ny;
	block[0] = binx;
	block[1] = biny;
	offset[0] = xstart;
	offset[1] = ystart;

	/* Check the subset of the input corresponding to the output */
	if ( (xstart < 0 || xstart + nx*binx > a->sci.data.tot_nx) ||
	     (ystart < 0 || ystart + ny*biny > a->sci.data.tot_ny) ) {
	    trlerror ("(trim2d)  subset is out of bounds:");
	    sprintf (MsgText,"         input is %d x %d pixels, output is %d x %d pixels",
		a->sci.data.tot_nx, a->sci.data.tot_ny, b->sci.data.tot_nx, b->sci.data.tot_ny);
	    trlmessage (MsgText);
		sprintf (MsgText, "        startx = (%d), starty = (%d), binx = %d, biny = %d.", 
                 xstart+1, ystart+1, binx, biny);
	    trlmessage (MsgText);
		return (status = SIZE_MISMATCH);
	}

    for (n = 0, j = ystart; n < ny; n++, j++) {
	    for (m = 0, i = xstart;  m < nx;  m++, i++) {
	        /* Extract the science array */
		    Pix(b->sci.data, m, n) = Pix(a->sci.data, i, j);

	        /* Extract the error array */
		    Pix(b->err.data, m, n) = Pix(a->err.data, i, j);

	        /* Extract the data quality array */
		    DQSetPix (b->dq.data, m, n, DQPix(a->dq.data, i, j));
        }
    }

	/* Copy header info only if requested... */
	if (update == YES) {
		/* Copy the headers. */
		copyHdr (b->globalhdr, a->globalhdr);
		if (hstio_err())
	    	return (status = 1001);
		copyHdr (&b->sci.hdr, &a->sci.hdr);
		if (hstio_err())
	    	return (status = 1001);
		copyHdr (&b->err.hdr, &a->err.hdr);
		if (hstio_err())
	    	return (status = 1001);
		copyHdr (&b->dq.hdr, &a->dq.hdr);
		if (hstio_err())
	    	return (status = 1001);

		/* Update the coordinate parameters that depend on the binning. */
		if (BinCoords (&a->sci.hdr, block, offset,
			&b->sci.hdr, &b->err.hdr, &b->dq.hdr))
	    	return (status);
	}
	return (status);
}

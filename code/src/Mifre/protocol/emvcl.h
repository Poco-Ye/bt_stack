#ifndef EMVCL_H
#define EMVCL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * polling card in field, check if there is a typeA or typeB card.
 *  
 * params:
 *        ptPcdInfo :  the PCD information resource structure pointer
 * retval:
 *        0 - ok
 *        others, failure or absent
 */
int EmvPolling( struct ST_PCDINFO *ptPcdInfo );

/**
 * ptPcdInfo collision detection.
 * 
 * params: 
 *        ptPcdInfo : ptPcdInfo information
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int EmvAntiSelect( struct ST_PCDINFO* ptPcdInfo );

/**
 * ptPcdInfo activating card.
 * 
 * params: 
 *        ptPcdInfo : ptPcdInfo information
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int EmvActivate( struct ST_PCDINFO* ptPcdInfo );

/**
 * removal card.
 * 
 * params: 
 *        ptPcdInfo       : ptPcdInfo information
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int EmvRemoval( struct ST_PCDINFO* ptPcdInfo );

#ifdef __cplusplus
}
#endif

#endif

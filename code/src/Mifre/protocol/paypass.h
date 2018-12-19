#ifndef PAYPASS_H
#define PAYPASS_H

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
int PayPassPolling( struct ST_PCDINFO *ptPcdInfo );

/**
 * ptPcdInfo collision detection.
 * 
 * params: 
 *        ptPcdInfo : ptPcdInfo information
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int PayPassAntiSelect( struct ST_PCDINFO* ptPcdInfo );

/**
 * ptPcdInfo activating card.
 * 
 * params: 
 *        ptPcdInfo : ptPcdInfo information
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int PayPassActivate( struct ST_PCDINFO* ptPcdInfo );

/**
 * removal card.
 * 
 * params: 
 *        ptPcdInfo : ptPcdInfo information
 * retval:
 *        0 - successfully.
 *        others, error.
 */
int PayPassRemoval( struct ST_PCDINFO* ptPcdInfo );

#ifdef __cplusplus
}
#endif

#endif

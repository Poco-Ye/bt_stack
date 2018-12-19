/*
 * Entry points for dhd utility application for platforms where the dhd utility
 * is not a standalone application.
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: dhdu_api.h,v 1.2 2008-12-01 22:56:59 Exp $
 */


#ifndef dhdu_api_h
#define dhdu_api_h

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Include Files ---------------------------------------------------- */
/* ---- Constants and Types ---------------------------------------------- */
/* ---- Variable Externs ------------------------------------------------- */
/* ---- Function Prototypes ---------------------------------------------- */


/****************************************************************************
* Function:   dhdu_main_args
*
* Purpose:    Entry point for dhd utility application. Uses same prototype
*             as standard main() functions - argc and argv parameters.
*
* Parameters: argc (in) Number of 'argv' arguments.
*             argv (in) Array of command-line arguments.
*
* Returns:    0 on success, else error code.
*****************************************************************************
*/
int dhdu_main_args(int argc, char **argv);


/****************************************************************************
* Function:   dhdu_main_str
*
* Purpose:    Alternate entry point for dhd utility application. This function
*             will tokenize the input command line string, before calling
*             dhdu_main_args() to process the command line arguments.
*
* Parameters: str (mod) Input command line string. Contents will be modified!
*
* Returns:    0 on success, else error code.
*****************************************************************************
*/
int dhdu_main_str(char *str);


#ifdef __cplusplus
	}
#endif

#endif  /* dhdu_api_h  */

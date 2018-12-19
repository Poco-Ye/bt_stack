/******************************************************************************
 *
 *  Copyright (C) 2014 The Android Open Source Project
 *  Copyright 2003 - 2004 Open Interface North America, Inc. All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/**********************************************************************************
  $Revision: #1 $
***********************************************************************************/

/** @file
@ingroup codec_internal
*/

/**@addgroup codec_internal*/
/**@{*/

#include "oi_codec_sbc_private.h"

const OI_CHAR* const OI_CODEC_SBC_FreqText[] =     { "SBC_FREQ_16000", "SBC_FREQ_32000", "SBC_FREQ_44100", "SBC_FREQ_48000" };
const OI_CHAR* const OI_CODEC_SBC_ModeText[] =     { "SBC_MONO", "SBC_DUAL_CHANNEL", "SBC_STEREO", "SBC_JOINT_STEREO" };
const OI_CHAR* const OI_CODEC_SBC_SubbandsText[] = { "SBC_SUBBANDS_4", "SBC_SUBBANDS_8" };
const OI_CHAR* const OI_CODEC_SBC_BlocksText[] =   { "SBC_BLOCKS_4", "SBC_BLOCKS_8", "SBC_BLOCKS_12", "SBC_BLOCKS_16" };
const OI_CHAR* const OI_CODEC_SBC_AllocText[] =    { "SBC_LOUDNESS", "SBC_SNR" };

#ifdef OI_DEBUG
#include <stdio.h> 
void OI_CODEC_SBC_DumpConfig(OI_CODEC_SBC_FRAME_INFO *frameInfo)
{
    LogPrintf("SBC configuration\n");
    LogPrintf("  enhanced:  %s\n", frameInfo->enhanced ? "TRUE" : "FALSE");
    LogPrintf("  frequency: %d\n", frameInfo->frequency);
    LogPrintf("  subbands:  %d\n", frameInfo->nrof_subbands);
    LogPrintf("  blocks:    %d\n", frameInfo->nrof_blocks);
    LogPrintf("  channels:  %d\n", frameInfo->nrof_channels);
    LogPrintf("  mode:      %s\n", OI_CODEC_SBC_ModeText[frameInfo->mode]);
    LogPrintf("  alloc:     %s\n", OI_CODEC_SBC_AllocText[frameInfo->alloc]);
    LogPrintf("  bitpool:   %d\n", frameInfo->bitpool);
}
#endif /* OI_DEBUG */

/**@}*/


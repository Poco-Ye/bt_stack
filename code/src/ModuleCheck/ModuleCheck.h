#ifndef _MODULECHECK_H
#define _MODULECHECK_H
#define MODULEMAX   9 //最大模块数
enum{MC_ICC = 0,MC_MAG,MC_RF,MC_KEYBOARD,MC_LCD,MC_MODEM,MC_ETHNET,MC_WNET};
/**
功能:显示模块检测菜单
iModule [in] 表示需显示的模块，数组内的元素需使用上述enum结构中的元素，
			 机型有哪些模块需要进行模块检测，则在数组中加入该模块。
			 数组中各模块的顺序决定了菜单中各项的顺序。
			 对于某些模块不固定的机型，需要分配一个足够大的数组，动态填入需检测的模块。
iModuleNum [in] 表示iModule中的前iModuleNum项会显示出来。
*/
int ModuleCheckMenu(int iModule[],int iModuleNum);
/**
功能:获取库版本信息
*/
void ModuleCheckGetVer(char Version[30]);
#endif


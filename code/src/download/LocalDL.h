#ifndef  _LOCALDL_H
#define  _LOCALDL_H

#define     LOCAL_DL_VER            10

#define     LOCAL_DL_PACK_LEN       8192
#define     LOCAL_DL_MAX_BAUD       230400


#define     HANDSHAKE_RECV          'Q'
#define     HANDSHAKE_SEND          'S'

#define     LDL_RET_SUCCESS         0
#define     LDL_RET_FINISH          1
#define     LDL_RET_ERROR           2
#define     LDL_RET_TIMEOUT         3
#define     LDL_RET_REHANDSHAKE     4

#define     LDL_TIMEOUT             15000
#define     LDL_STX                 0x02

#define     LDL_COMMAND             0x80

#define     LDLCMD_BAUD             0xA1        //  设置通讯波特率
#define     LDLCMD_TERMINFO         0xA2        //  查询POS终端信息
#define     LDLCMD_APPINFO          0xA3        //  查询应用信息
#define     LDLCMD_REBUILD          0xA4        //  重建POS终端文件系统
#define     LDLCMD_SETTIME          0xA5        //  设置POS终端时间
#define     LDLCMD_DLPUK            0xA6        //  下载用户公钥
#define     LDLCMD_DLAPP            0xA7        //  下载应用程序文件请求
#define     LDLCMD_DLFONT           0xA8        //  下载字库文件请求
#define     LDLCMD_DLPARA           0xA9        //  下载参数文件请求
#define     LDLCMD_DLDMR            0xAA        //  下载LCD注册文件请求
#define     LDLCMD_DATA             0xAB        //  下载文件数据
#define     LDLCMD_COMPRESSDATA     0xAC        //  以压缩方式下载文件数据
#define     LDLCMD_WRITEDATA        0xAD        //  写入文件
#define     LDLCMD_DELAPP           0xAE        //  删除应用程序
#define     LDLCMD_QUIT             0xBF        //  下载完成
#define     LDLCMD_OPTIONCONF       0xC0        //  下载终端配置表


#define     LD_OK                   0x00        //  成功
#define     LDERR_GENERIC           0x01        //  失败
#define     LDERR_HAVEMOREDATA      0x02        //  还有下一个包,请求的数据未能一次全部返回
#define     LDERR_BAUDERR           0x03        //  波特率不支持
#define     LDERR_INVALIDTIME       0x04        //  非法时间
#define     LDERR_CLOCKHWERR        0x05        //  时钟硬件错误
#define     LDERR_SIGERR            0x06        //  验证签名失败
#define     LDERR_TOOMANYAPP        0x07        //  应用太多，不能下载更多应用
#define     LDERR_TOOMANYFILES      0x08        //  文件太多，不能下载更多文件
#define     LDERR_NOAPP             0x09        //  指定应用不存在
#define     LDERR_UNKNOWNAPP        0x0A        //  不可识别的应用类型
#define     LDERR_SIGTYPEERR        0x0B        //  签名的数据类型和下载请求类型不一致
#define     LDERR_SIGAPPERR         0x0C        //  签名的数据所属的应用名和下载请求应用名不一致
#define     LDERR_WRITEFILEFAIL     0x10        //  文件写入失败
#define     LDERR_NOSPACE           0x11        //  没有足够的空间
#define     LDERR_UNSUPPORTEDCMD    0xFF        //  不支持该命令
#define 	LDERR_FWVERSION			0x14		// 下载的固件版本不正确

#endif


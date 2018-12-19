#define uchar unsigned char
static const uchar DESCRIPTOR_DEVICE[18]=
{
		18,//length
		1,//descriptor type:device
		0,//version l
		2,//version h
		0,//class
		0,//sub-class,offset 5
		0,//protocol
		64,//ep0 packet size
		0x34,//vendor l
		0x12,//vendor h
		0x01,//0x01,product l,offset 10
		0x01,//product h
		0x00,//release l,01-first version,101-revised version
		0x03,//release h
		1,//manufacturer string index
		2,//product string index,offset 15
		0,//serial number string index
		1//configuration numbers
};

static const uchar DESCRIPTOR_CONFIG[46]=
{
//config define
	9,//length
	2,//descriptor type:config
	46,//total length,l
	0,//total length,h
	1,//interface numbers
	1,//configuration index,offset 5
	4,//string descriptor index for this config
	//--Attributes: D7-reserved, set to 1. (USB 1.0 Bus Powered)
	//              D6-self powered, D5-remote wakeup, D4..0-reserved to 0
	0xc0,
	3,//maximum power drawn from bus in 2 mA units

//interface define
	9,//length
	4,//descriptor type:interface,offset 10
	0,//interface index
	0,//alternate setting
	2,//endpoint numbers excluding ep0
	0xFF,//interface class,0x08:mass storage,0xff:vendor specific
	0x00,//interface subclass,0x06,offset 15
	0x00,//interface protocol,0x50,
	0,//string descriptor index for this interface

//endpoint1 define,used as BULK-IN
	7,//length
	5,//descriptor type:endpoint
	0x81,//endpoint address,D7 is direction,0:OUT,1:IN,offset 20
	2,//attribute,0-Control,1-Isochronous,2-Bulk,3-Interrupt
	64,//max packet size,l
	0,//max packet size,h
	0,//polling Interval(ms)

//Endpoint2 define,used as BULK-OUT
	7,//length,offset 25
	5,//descriptor type:endpoint
	0x01,//endpoint address,D7 is direction,0:OUT,1:IN
	0x02,//attribute,0-Control,1-Isochronous,2-Bulk,3-Interrupt
	64,//max packet size,l
	0,//max packet size,h,offset 30
	0,//polling Interval(ms)

//Endpoint3 define,used as BULK-OUT
	7,//length
	5,//descriptor type:endpoint
	0x82,//endpoint address,D7 is direction,0:OUT,1:IN
	0x02,//attribute,0-Control,1-Isochronous,2-Bulk,3-Interrupt,offset 35
	64,//max packet size,l
	0,//max packet size,h
	0,//polling Interval(ms)

//Endpoint4 define,used as BULK-IN
	7,//length
	5,//descriptor type:endpoint,offset 40
	0x02,//endpoint address,D7 is direction,0:OUT,1:IN
	0x02,//attribute,0-Control,1-Isochronous,2-Bulk,3-Interrupt
	64,//max packet size,l
	0,//max packet size,h,offset 30
	0,//polling Interval(ms),offset 45
};

static const uchar DESCRIPTOR_STR_0[4]=//string descriptor of LANGUAGE_ID
{
	4,
	3,
	0x09,0x04
};

static const uchar DESCRIPTOR_STR_1[8]=//string descriptor of manufacturer
{
	8,
	3,
	'P',0,
	'A',0,
	'X',0
};

static const uchar DESCRIPTOR_STR_2[24]=//string descriptor of product
{
	24,//length
	3,//descriptor type:string
	'S',0,
	'8',0,
	'0',0,
	' ',0,
	' ',0,
	'D',0,
	'E',0,
	'V',0,
	'I',0,
	'C',0,
	'E',0
};

static const uchar DESCRIPTOR_STR_4[8]=//string descriptor of config
{
	8,
	3,
	'#',0,
	'0',0,
	'2',0
};


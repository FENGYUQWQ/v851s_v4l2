// #include <unistd.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <fcntl.h>
// #include <sys/ioctl.h>
// #include <linux/types.h>
// #include <linux/videodev2.h>
// #include <malloc.h>
// #include <math.h>
// #include <string.h>
// #include <sys/mman.h>
// #include <errno.h>
// #include <assert.h>
 
// //#define _GNU_SOURCE 
// //#define __USE_GNU
// #include <sched.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <fcntl.h>
// #include <string.h>
// #include <stdlib.h>
// #include <poll.h>
// #include <sys/mman.h>
// #include <sys/sysinfo.h>
// #include <sys/time.h>
// #include <stdio.h>
 
// #include <endian.h>
// #include <errno.h>
// #include <fcntl.h>
// #include <getopt.h>
// #include <pthread.h>
// #include <signal.h>
// #include <stdint.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>


#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include <linux/videodev2.h>

typedef struct rxvideo_para
{
    char dev_path[32];
    int video_w;
    int video_h;
	int video_c;
}st_rxvideo_para;
 
st_rxvideo_para g_capture_video_para;
 
typedef enum
{
    INPUT,
    SRC_W,
    SRC_H,
	SRC_C,
	BUF_C,
	PIXEL_FORMAT,
	OUTPUT,
	HELP,
    INVALID,
}e_argu_type;
 
typedef struct argument
{
    char option[8];
    char type[16];
    e_argu_type argu;
    char desc[64];
}st_argument;
 
static const st_argument argu_type_mapping[] =
{
    {
        "-i",  "--input",   INPUT,
        "capture video path"
    },
    {
        "-w",  "--srcw",  SRC_W,
        "SRC_W"
    },
    {
        "-h",  "--srch",  SRC_H,
        "SRC_H"
    },
	{
        "-c",  "--srcc",  SRC_C,
        "SRC_C"
    },
#if 0
    {
        "-p",  "--pixel_format",  PIXEL_FORMAT,
        "0: YUV420SP, 1:YVU420SP, 3:YUV420P"
    },
    {
        "-o",  "--output",  OUTPUT,
        "output file path"
    },
#endif
	{
        "-he",  "--help",    HELP,
        "print this help"
    },
};
 
static e_argu_type get_argu_type(char* name)
{
    int i = 0;
    int num = sizeof(argu_type_mapping) / sizeof(st_argument);
	
    while (i < num)
    {
        if ((0 == strcmp(argu_type_mapping[i].type, name)) ||
                ((0 == strcmp(argu_type_mapping[i].option, name)) &&
                 (0 != strcmp(argu_type_mapping[i].option, "--"))))
        {
            return argu_type_mapping[i].argu;
        }
        i++;
    }
    return INVALID;
}
 
static void print_demo_usage(void)
{
    int i = 0;
    int num = sizeof(argu_type_mapping) / sizeof(st_argument);
	
    printf("Usage:\n");
    while (i < num)
    {
        printf("%s %-32s %s\n", argu_type_mapping[i].option, argu_type_mapping[i].type, argu_type_mapping[i].desc);
        i++;
    }
}
 
static void parse_argu_type(st_rxvideo_para* video_para, char* argument, char* value)
{
    e_argu_type argu_type;
 
    argu_type = get_argu_type(argument);
 
    switch (argu_type)
    {
    case HELP:
        print_demo_usage();
        exit(-1);
    case INPUT:
        memset(video_para->dev_path, 0, sizeof(video_para->dev_path));
        sscanf(value, "%31s", video_para->dev_path);
        printf("get input file: %s\n", video_para->dev_path);
        break;
	case SRC_W:
		sscanf(value, "%32u", &video_para->video_w);
		printf("get srcW: %d\n", video_para->video_w);
		break;
	case SRC_H:
		sscanf(value, "%32u", &video_para->video_h);
		printf("get srcH: %d\n", video_para->video_h);
		break;
	case SRC_C:
		sscanf(value, "%32u", &video_para->video_c);
		printf("get srcC: %d\n", video_para->video_c);
		break;
    default:
        printf("unknowed argument :  %s\n", argument);
        exit(-1);
    }
}
 
#define DEFAULT_DEV_PATH	"/dev/video0"
 
#define BUF_NUM				(4) 
#define CAPTURE_IMG_NUM		(30) 
 
#define MPLANE_WIDTH	(1920)       
#define MPLANE_HEIGHT	(1088) 
 
//定义一个结构体来映射每个缓冲帧
struct user_img
{
	void *start;
	int length;
};
 
struct user_img *usr_buf_video;
static unsigned int n_buffer_video = 0;
static unsigned int nplanes_video = 0;
 
int init_mmap_video(int fd)
{
	int i = 0;
	struct v4l2_requestbuffers reqbufs;
 	struct v4l2_buffer buf;
 
	memset(&reqbufs, 0, sizeof(reqbufs));
	reqbufs.count = BUF_NUM;
	reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;    
	reqbufs.memory = V4L2_MEMORY_MMAP;	
	
	//Step6: 向驱动申请帧缓存
	/*
	v4l2_requestbuffers结构中定义了缓存的数量，驱动会据此申请对应数量的视频缓存。
	多个缓存可以用于建立FIFO，来提高视频采集的效率。控制命令VIDIOC_REQBUFS。	   	
	功能：
		请求V4L2驱动分配视频缓冲区（申请V4L2视频驱动分配内存），V4L2是视频设备的驱动层，位于内核空间，
		所以通过VIDIOC_REQBUFS控制命令字申请的内存位于内核空间，应用程序不能直接访问，
		需要通过调用mmap内存映射函数把内核空间内存映射到用户空间后，应用程序通过访问用户空间地址来访问内核空间。	
	参数说明：
		参数类型为V4L2的申请缓冲区数据结构体类型struct v4l2_requestbuffers  ；
	返回值说明： 
		执行成功时，函数返回值为 0；V4L2驱动层分配好了视频缓冲区；
	*/
	
	/* VIDIOC_REQBUFS: frame buffer alloc by kernel and tell userspace how many frames had alloc */
	if(-1 == ioctl(fd, VIDIOC_REQBUFS, &reqbufs)) //把VIDIOC_REQBUFS中分配的数据缓存转换成物理地址  
	{
		printf("Fail to ioctl 'VIDIOC_REQBUFS'");
		exit(EXIT_FAILURE);
	}
 
	//Step7: 获取每个缓存的信息，并mmap到用户空间
	//应用程序和设备有三种交换数据的方法，直接 read/write、内存映射(memory mapping)和用户指针.
	
	n_buffer_video = reqbufs.count;
	printf("n_buffer_video = %d\n", n_buffer_video);
	
	usr_buf_video = (struct user_img *)calloc(reqbufs.count, sizeof(struct user_img));
	if(NULL == usr_buf_video)
	{
		printf("[%s][%d] Out of memory\n", __FUNCTION__, __LINE__);
		exit(-1);
	}
 
	/*map kernel frame buffer to user process*/
	for(i = 0; i < n_buffer_video; ++i)
	{
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
 
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		memset(planes, 0, VIDEO_MAX_PLANES * sizeof(struct v4l2_plane));
		/* when type=V4L2_BUF_TYPE_VIDEO_CAPTURE, length means frame size
		 * when type=V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, length means how many planes */
		buf.length = nplanes_video;
		buf.m.planes = planes;
		
		/* VIDIOC_QUERYBUF: get the frame buffer size and physical address since we need it mmmap to userspace */
		if(-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) //获取到对应index的缓存信息，此处主要利用length信息及offset信息来完成后面的mmap操作
		{
			printf("Fail to ioctl : VIDIOC_QUERYBUF");
			exit(EXIT_FAILURE);
		}
 
		/* FIXME: usr_buf[0].start=frame addr but not planes[0].m.mem_offset
		 * usr_buf[0].start = planes[0].m.mem_offset + planes[1].m.mem_offset + ... planes[nplanes-1].m.mem_offset
		 * in this case nplanes=1, So frame buffer has only 1 plane
		*/
 
		/*
		addr   映射起始地址，一般为NULL ，让内核自动选择
		length 被映射内存块的长度
		prot   标志映射后能否被读写，其值为PROT_EXEC,PROT_READ,PROT_WRITE, PROT_NONE
		flags  确定此内存映射能否被其他进程共享，MAP_SHARED,MAP_PRIVATE
		fd,offset, 确定被映射的内存地址 返回成功映射后的地址，不成功返回MAP_FAILED ((void*)-1)
		*/
		
		usr_buf_video[i].length = buf.m.planes[0].length;
		usr_buf_video[i].start = (char *)mmap(NULL, buf.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[0].m.mem_offset);
		printf("index[%d]  length=%d, phyaddr=%p, virtaddr=%p\n",
			buf.index, usr_buf_video[i].length, buf.m.planes[0].m.mem_offset, usr_buf_video[i].start);
 
		if(MAP_FAILED == usr_buf_video[i].start) 
		{
			printf("Fail to mmap");
			exit(EXIT_FAILURE);
		}
 
		//Step8: 开始采集视频前，将缓冲帧放入缓冲队列
		if(-1 == ioctl(fd, VIDIOC_QBUF, &buf))
		{
			printf("Fail to ioctl 'VIDIOC_QBUF'");
			exit(EXIT_FAILURE);
		}
	}
	
	return 0;
}
 
int open_camera_video(void)
{
	int fd = -1;
	struct v4l2_input inp;
 
	//Step1: 打开设备文件
 
	fd = open(g_capture_video_para.dev_path, O_RDWR | O_NONBLOCK, 0); //使用非阻塞模式打开摄像头设备，
												   //应用程序能够使用阻塞模式或非阻塞模式打开视频设备，
												   //如果使用非阻塞模式调用视频设备，即使尚未捕获到信息，
												   //驱动依旧会把缓存（DQBUFF）里的东西返回给应用程序。
	if(fd < 0) 
	{	
		printf("%s open err \n", g_capture_video_para.dev_path);
		exit(EXIT_FAILURE);
	}
	
	printf("[%s][%d] open %s success %d !!!\n", __FUNCTION__, __LINE__, g_capture_video_para.dev_path, fd);
 
 	/* must act VIDIOC_S_INPUT otherwise select will timeout */
	/*
	一个视频设备可以有多个视频输入。如果只有一路输入，这个功能可以没有。
	VIDIOC_G_INPUT 和 VIDIOC_S_INPUT 用来查询和选择当前的 input，一个 video 设备节点可能对应多个视频源，
	比如 saf7113 可以最多支持四路 cvbs 输入，如果上层想在四个cvbs视频输入间切换，
	那么就要调用 ioctl(fd, VIDIOC_S_INPUT, &input) 来切换。
	VIDIOC_G_INPUT and VIDIOC_G_OUTPUT 返回当前的 video input和output的index.
	*/
 
	//Step2: 设置视频设备对应视频源, 即选择视频输入
	inp.index = 1;
	if (-1 == ioctl(fd, VIDIOC_S_INPUT, &inp)) 
	{
		printf("VIDIOC_S_INPUT fail\n");
	}
 
	return fd;
}
 
int init_camera_video(int fd)
{
	struct v4l2_capability cap;
	struct v4l2_format tv_fmt;
	struct v4l2_fmtdesc fmtdesc;
	int ret = 0;
	int i = 0;
 
	//Step3: 取得设备的capability
	/* check video devive driver capability : 查询设备属性, 看看设备具有什么功能，比如是否具有视频输入特性 */
	memset(&cap, 0, sizeof(cap));
	ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if(ret < 0) 
	{
		printf("fail to ioctl VIDEO_QUERYCAP \n");
		exit(EXIT_FAILURE);
	}
 
	/*judge wherher or not to be a video-get device, some driver forget add to this support list */
	if(!(cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE)) //V4L2_BUF_TYPE_VIDEO_CAPTURE 即视频捕捉模式
	{
		printf("[%s][%d]: The Current device is not a video capture device\n", __FUNCTION__, __LINE__);
	}
	
	if(!(cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) 
	{
		printf("[%s][%d]: The Current device is not V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE\n", __FUNCTION__, __LINE__);
	}
 
	#if 0
	//获取成功，检查是否有视频捕获功能
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE))
	{
    	//printf("[%s][%d]: The Current device does not support capture device\n", __FUNCTION__, __LINE__);
    	exit(EXIT_FAILURE);
	}
	#endif
	
	/*judge whether or not to supply the form of video stream*/
	if(!(cap.capabilities & V4L2_CAP_STREAMING)) //具备数据流控制模式
	{
		//printf("[%s][%d]: The Current device does not support streaming i/o\n", __FUNCTION__, __LINE__);
		exit(EXIT_FAILURE);
	}
 
	printf("Driver Name:%s\nCard Name:%s\nBus info:%s\nDriver Version:%u.%u.%u\n",cap.driver,cap.card,cap.bus_info,(cap.version>>16)&0XFF, (cap.version>>8)&0XFF,cap.version&0XFF);
#if 1	
	//Step4: 查询当前设备支持的视频格式
	/*display the format device support*/
	memset(&fmtdesc, 0, sizeof(fmtdesc));
	fmtdesc.index = 0;                 /* the number to check */
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
 
//	printf("find V4L2_PIX_FMT_NV21 0x%x\n", V4L2_PIX_FMT_NV21);
 
	for (i = 0; i < 16; i++) 
	{
		fmtdesc.index = i;
		if (-1 == ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) //获取当前驱动支持的视频格式
		{
			break;
		}
		
		printf("Index = %d, Name = %s, Format = %c%c%c%c\n", i, fmtdesc.description, 	(fmtdesc.pixelformat&0x000000FF),
																						(fmtdesc.pixelformat&0x0000FF00)>>8,
																						(fmtdesc.pixelformat&0x00FF0000)>>16,
																						(fmtdesc.pixelformat&0xFF000000)>>24	);	
	}
	printf("\n");
#endif
 
	//Step5: 设置视频捕获格式、获取实际的视频格式
	/*
	v4l2_format 结构体用来设置摄像头的视频制式、帧格式等，
	在设置这个参数时应先填好v4l2_format的各个域，
	如type（传输流类型），fmt.pix.width(宽)，fmt.pix.heigth(高)，fmt.pix.field(采样区域，如隔行采样)，
	fmt.pix.pixelformat(采样类型，如 YUV4:2:2)，然后通过 VIDIO_S_FMT 操作命令设置视频捕捉格式。
	*/
 
	/*set the form of camera capture data*/
	tv_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; //帧类型
	tv_fmt.fmt.pix_mp.width = g_capture_video_para.video_w;//MPLANE_WIDTH;
	tv_fmt.fmt.pix_mp.height = g_capture_video_para.video_h;//MPLANE_HEIGHT;
	/* vidioc_try_fmt_vid_cap_mplane() */
	tv_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV21; //视频设备使用
	tv_fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	
	if(ioctl(fd, VIDIOC_S_FMT, &tv_fmt) < 0) //设置当前驱动的视频捕获格式 
	{
		printf("VIDIOC_S_FMT set err\n");
		close(fd);
		exit(-1);
	}
 
	if(ioctl(fd, VIDIOC_G_FMT, &tv_fmt) < 0) //读取当前驱动的视频捕获格式
	{
		printf("VIDIOC_G_FMT get err\n");
		close(fd);
		exit(-1);
	} 
	else 
	{
		nplanes_video = tv_fmt.fmt.pix_mp.num_planes;
		printf("VIDIOC_G_FMT : resolution = %d*%d num_planes = %d\n", tv_fmt.fmt.pix_mp.width, tv_fmt.fmt.pix_mp.height, tv_fmt.fmt.pix_mp.num_planes);
	}
 
#if 0
	//例：检查是否支持某种帧格式
	struct v4l2_format tv_fmt; 
	tv_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; 
	tv_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_Y16; 
	if(ioctl(fd, VIDIOC_TRY_FMT, &tv_fmt) == -1)
	{
		printf("not support format Y16!\n");
	}
#endif
 
	/*
	Note: 如果该视频设备驱动不支持你所设定的图像格式，视频驱动会重新修改struct v4l2_format结构体变量的值为该视频设备所支持的图像格式，
	      所以在程序设计中，设定完所有的视频格式后，要获取实际的视频格式，要重新读取struct v4l2_format结构体变量。　
	*/
 
	init_mmap_video(fd);
	
	return 0;
} 
 
int start_capture_video(int fd)
{
	//Step9: 开始采集视频，打开设备视频流
	enum v4l2_buf_type type;
	
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if(-1 == ioctl(fd, VIDIOC_STREAMON, &type)) //开始视频显示函数 
	{
		printf("VIDIOC_STREAMON");
		close(fd);
		exit(EXIT_FAILURE);
	}
 
	return 0;
}
 
int process_image_video(void *addr, int length)
{
	FILE *fp;
 
	static int num = 0;
 
	char image_name[32];
	sprintf(image_name, "pic/v4l2_yuv_video%d.yuv", num++);
	if((fp = fopen(image_name, "w")) == NULL)
	{
		printf("Fail to fopen");
		exit(EXIT_FAILURE);
	}
	
	fwrite(addr, length, 1, fp);
	usleep(500);
	fclose(fp);
	
	return 0;
}
 
int read_frame_video(int fd)
{
	struct v4l2_buffer buf;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
 
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.length = nplanes_video;
	buf.m.planes = planes;
 
	//Step10: 取出FIFO缓存中已经采样的帧缓存
	//将已经捕获好视频的内存拉出已捕获视频的队列
	if(-1 == ioctl(fd, VIDIOC_DQBUF,&buf))
	{
		printf("Fail to ioctl 'VIDIOC_DQBUF'");
		exit(EXIT_FAILURE);
	}
	
	assert(buf.index < n_buffer_video);
 
	printf("[video0] [%d] %ld, %ld\n", buf.index, buf.timestamp.tv_sec, buf.timestamp.tv_usec);
	process_image_video(usr_buf_video[buf.index].start, usr_buf_video[buf.index].length);
 
	//Step11: 将刚刚处理完的缓冲重新入队列尾，这样可以循环采集
	if(-1 == ioctl(fd, VIDIOC_QBUF, &buf)) 
	{
		printf("Fail to ioctl 'VIDIOC_QBUF'");
		exit(EXIT_FAILURE);
	}
	
	return 1;
}
 
 
int mainloop_video(int fd)
{
	int count = g_capture_video_para.video_c;//CAPTURE_IMG_NUM;
	
	while(count-- > 0)
	{
		for(;;) 
		{
			fd_set fds;
			struct timeval tv;
			int r;
 
			FD_ZERO(&fds);
			FD_SET(fd, &fds);
 
			/*Timeout*/
			tv.tv_sec = 5;
			tv.tv_usec = 0;
			r = select(fd + 1, &fds, NULL, NULL, &tv);
			
			if(-1 == r) 
			{
				if(EINTR == errno)
				{
					continue;
				}
					
				printf("Fail to select");
				exit(EXIT_FAILURE);
			}
			if(0 == r)
			{
				printf("select Timeout\n");
				exit(-1);
			}
 
			if(read_frame_video(fd))
			{
				break;
			}
		}
	}
	
	return 0;
}
 
void stop_capture_video(int fd)
{
	//Step12: 停止视频的采集
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if(-1 == ioctl(fd, VIDIOC_STREAMOFF, &type)) //结束视频显示函数 
	{
		printf("Fail to ioctl 'VIDIOC_STREAMOFF'");
		exit(EXIT_FAILURE);
	}
}
 
void close_camera_device_video(int fd)
{
	//Step13: 解除映射
	unsigned int i = 0;
	for(i = 0; i < n_buffer_video; i++)
	{
		if(-1 == munmap(usr_buf_video[i].start, usr_buf_video[i].length))
		{
			exit(-1);
		}
	}
 
	free(usr_buf_video);
 
	if(-1 == close(fd))
	{
		printf("Fail to close fd");
		exit(EXIT_FAILURE);
	}
}
 
	int fd_video = -1;

 #include "isp.h"

int main(int argc, char** argv)
{


	memset(&g_capture_video_para, 0, sizeof(g_capture_video_para));
	


	sprintf(g_capture_video_para.dev_path, "%s", DEFAULT_DEV_PATH);
	g_capture_video_para.video_w = MPLANE_WIDTH;
	g_capture_video_para.video_h = MPLANE_HEIGHT;
	g_capture_video_para.video_c = CAPTURE_IMG_NUM;
	
	int index = 0;
    if (argc >= 2)
    {
        for (index = 1; index < (int)argc; index += 2)
        {
            parse_argu_type(&g_capture_video_para, argv[index], argv[index + 1]);
        }
    }
	




	fd_video = open_camera_video();
	media_dev_init();
	isp_init(0);
	isp_run(0);

	init_camera_video(fd_video);
	start_capture_video(fd_video);
 
	mainloop_video(fd_video);
 
	stop_capture_video(fd_video);
	close_camera_device_video(fd_video);

 	isp_pthread_join(0);
	isp_exit(0);
	media_dev_exit();
	printf("[%d] %s exit !!!\n", __LINE__, __FUNCTION__);
 
    return 0;
}








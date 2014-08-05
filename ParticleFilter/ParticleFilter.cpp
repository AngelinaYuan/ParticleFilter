//#include <opencv2/imgproc/imgproc.hpp>  
//#include <opencv2/core/core.hpp>
//#include <opencv2/highgui/highgui.hpp>  
#include <opencv2/opencv.hpp>
#include<time.h>
#include<iostream>
using namespace std;
using namespace cv;


#define B(image,x,y) ((uchar*)(image->imageData + image->widthStep*(y)))[(x)*3]		//B
#define G(image,x,y) ((uchar*)(image->imageData + image->widthStep*(y)))[(x)*3+1]	//G
#define R(image,x,y) ((uchar*)(image->imageData + image->widthStep*(y)))[(x)*3+2]	//R
#define S(image,x,y) ((uchar*)(image->imageData + image->widthStep*(y)))[(x)]	
#define  Num 10  //֡��ļ��
#define  T 40    //Tf
#define Re 30     //
#define ai 0.08   //ѧϰ��

#define CONTOUR_MAX_AREA 10000
#define CONTOUR_MIN_AREA 50

# define R_BIN      8  /* ��ɫ������ֱ��ͼ���� */
# define G_BIN      8  /* ��ɫ������ֱ��ͼ���� */
# define B_BIN      8  /* ��ɫ������ֱ��ͼ���� */ 

# define R_SHIFT    5  /* ������ֱ��ͼ������Ӧ */
# define G_SHIFT    5  /* ��R��G��B��������λ�� */
# define B_SHIFT    5  /* log2( 256/8 )Ϊ�ƶ�λ�� */

/*
����Park and Miller��������[0,1]֮����ȷֲ���α�����
�㷨��ϸ��������
[1] NUMERICAL RECIPES IN C: THE ART OF SCIENTIFIC COMPUTING.
Cambridge University Press. 1992. pp.278-279.
[2] Park, S.K., and Miller, K.W. 1988, Communications of the ACM, 
vol. 31, pp. 1192�C1201.
*/

#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836
#define MASK 123459876


typedef struct __SpaceState {  /* ״̬�ռ���� */
	int xt;               /* x����λ�� */
	int yt;               /* x����λ�� */
	float v_xt;           /* x�����˶��ٶ� */
	float v_yt;           /* y�����˶��ٶ� */
	int Hxt;              /* x����봰�� */
	int Hyt;              /* y����봰�� */
	float at_dot;         /* �߶ȱ任�ٶ� */
} SPACESTATE;


bool pause=false;//�Ƿ���ͣ
bool track = false;//�Ƿ����
IplImage *curframe=NULL; 
IplImage *pBackImg=NULL;
IplImage *pFrontImg=NULL;
IplImage *pTrackImg =NULL;
unsigned char * img;//��iplimg�ĵ�char*  ���ڼ���
int xin,yin;//����ʱ��������ĵ�
int xout,yout;//����ʱ�õ���������ĵ�
int Wid,Hei;//ͼ��Ĵ�С
int WidIn,HeiIn;//����İ������
int WidOut,HeiOut;//����İ������

long ran_seed = 802163120; /* ��������ӣ�Ϊȫ�ֱ���������ȱʡֵ */

float DELTA_T = (float)0.05;    /* ֡Ƶ������Ϊ30��25��15��10�� */
int POSITION_DISTURB = 20; //15     /* λ���Ŷ�����   */
float VELOCITY_DISTURB = 40.0;  /* �ٶ��Ŷ���ֵ   */
float SCALE_DISTURB = 0.0;      /* �������Ŷ����� */
float SCALE_CHANGE_D = (float)0.001;   /* �߶ȱ任�ٶ��Ŷ����� */

int NParticle = 100;       /* ���Ӹ���   */
float * ModelHist = NULL; /* ģ��ֱ��ͼ */
SPACESTATE * states = NULL;  /* ״̬���� */
float * weights = NULL;   /* ÿ�����ӵ�Ȩ�� */
int nbin;                 /* ֱ��ͼ���� */
float Pi_Thres = (float)0.90; /* Ȩ����ֵ   */
float Weight_Thres = (float)0.0001;  /* ���Ȩ����ֵ�������ж��Ƿ�Ŀ�궪ʧ */

int bSelectObject = 0;
Point origin;
Rect selection;
/*
����������
һ������ϵͳʱ�����������ã�Ҳ����ֱ�Ӵ���һ��long������
*/
long set_seed( long setvalue )
{
	if ( setvalue != 0 ) /* �������Ĳ���setvalue!=0�����ø���Ϊ���� */
		ran_seed = setvalue;
	else                 /* ��������ϵͳʱ��Ϊ������ */
	{
		ran_seed = time(NULL);
	}
	return( ran_seed );
}

/*
����һ��ͼ����ĳ������Ĳ�ɫֱ��ͼ�ֲ�
���������
int x0, y0��           ָ��ͼ����������ĵ�
int Wx, Hy��           ָ��ͼ������İ���Ͱ��
unsigned char * image��ͼ�����ݣ����������ң��������µ�˳��ɨ�裬
��ɫ���д���RGB, RGB, ...
(���ߣ�YUV, YUV, ...)
int W, H��             ͼ��Ŀ��͸�
���������
float * ColorHist��    ��ɫֱ��ͼ����ɫ��������
i = r * G_BIN * B_BIN + g * B_BIN + b����
int bins��             ��ɫֱ��ͼ������R_BIN*G_BIN*B_BIN������ȡ8x8x8=512��
*/
void CalcuColorHistogram( int x0, int y0, int Wx, int Hy, 
						 unsigned char * image, int W, int H,
						 float * ColorHist, int bins )
{
	int x_begin, y_begin;  /* ָ��ͼ����������Ͻ����� */
	int y_end, x_end;
	int x, y, i, index;
	int r, g, b;
	float k, r2, f;
	int a2;

	for ( i = 0; i < bins; i++ )     /* ֱ��ͼ����ֵ��0 */
		ColorHist[i] = 0.0;
	/* �������������x0, y0��ͼ�����棬���ߣ�Wx<=0, Hy<=0 */
	/* ��ʱǿ�����ɫֱ��ͼΪ0 */
	if ( ( x0 < 0 ) || (x0 >= W) || ( y0 < 0 ) || ( y0 >= H ) 
		|| ( Wx <= 0 ) || ( Hy <= 0 ) ) return;

	x_begin = x0 - Wx;               /* ����ʵ�ʸ߿���������ʼ�� */
	y_begin = y0 - Hy;
	if ( x_begin < 0 ) x_begin = 0;
	if ( y_begin < 0 ) y_begin = 0;
	x_end = x0 + Wx;
	y_end = y0 + Hy;
	if ( x_end >= W ) x_end = W-1;
	if ( y_end >= H ) y_end = H-1;
	a2 = Wx*Wx+Hy*Hy;                /* ����˺����뾶ƽ��a^2 */
	f = 0.0;                         /* ��һ��ϵ�� */
	for ( y = y_begin; y <= y_end; y++ )
		for ( x = x_begin; x <= x_end; x++ )
		{
			r = image[(y*W+x)*3] >> R_SHIFT;   /* ����ֱ��ͼ */
			g = image[(y*W+x)*3+1] >> G_SHIFT; /*��λλ������R��G��B���� */
			b = image[(y*W+x)*3+2] >> B_SHIFT;
			index = r * G_BIN * B_BIN + g * B_BIN + b;
			r2 = (float)(((y-y0)*(y-y0)+(x-x0)*(x-x0))*1.0/a2); /* ����뾶ƽ��r^2 */
			k = 1 - r2;   /* �˺���k(r) = 1-r^2, |r| < 1; ����ֵ k(r) = 0 */
			f = f + k;
			ColorHist[index] = ColorHist[index] + k;  /* ������ܶȼ�Ȩ��ɫֱ��ͼ */
		}
		for ( i = 0; i < bins; i++ )     /* ��һ��ֱ��ͼ */
			ColorHist[i] = ColorHist[i]/f;

		return;
}

/*
����Bhattacharyyaϵ��
���������
float * p, * q��      ������ɫֱ��ͼ�ܶȹ���
int bins��            ֱ��ͼ����
����ֵ��
Bhattacharyyaϵ��
*/
float CalcuBhattacharyya( float * p, float * q, int bins )
{
	int i;
	float rho;

	rho = 0.0;
	for ( i = 0; i < bins; i++ )
		rho = (float)(rho + sqrt( p[i]*q[i] ));

	return( rho );
}


/*# define RECIP_SIGMA  3.98942280401  / * 1/(sqrt(2*pi)*sigma), ����sigma = 0.1 * /*/
# define SIGMA2       0.02           /* 2*sigma^2, ����sigma = 0.1 */

float CalcuWeightedPi( float rho )
{
	float pi_n, d2;

	d2 = 1 - rho;
	//pi_n = (float)(RECIP_SIGMA * exp( - d2/SIGMA2 ));
	pi_n = (float)(exp( - d2/SIGMA2 ));

	return( pi_n );
}

/*
����Park and Miller��������[0,1]֮����ȷֲ���α�����
�㷨��ϸ��������
[1] NUMERICAL RECIPES IN C: THE ART OF SCIENTIFIC COMPUTING.
Cambridge University Press. 1992. pp.278-279.
[2] Park, S.K., and Miller, K.W. 1988, Communications of the ACM, 
vol. 31, pp. 1192�C1201.
*/

float ran0(long *idum)
{
	long k;
	float ans;

	/* *idum ^= MASK;*/      /* XORing with MASK allows use of zero and other */
	k=(*idum)/IQ;            /* simple bit patterns for idum.                 */
	*idum=IA*(*idum-k*IQ)-IR*k;  /* Compute idum=(IA*idum) % IM without over- */
	if (*idum < 0) *idum += IM;  /* flows by Schrage��s method.               */
	ans=AM*(*idum);          /* Convert idum to a floating result.            */
	/* *idum ^= MASK;*/      /* Unmask before return.                         */
	return ans;
}


/*
���һ��[0,1]֮����ȷֲ��������
*/
float rand0_1()
{
	return( ran0( &ran_seed ) );
}



/*
���һ��x - N(u,sigma)Gaussian�ֲ��������
*/
float randGaussian( float u, float sigma )
{
	float x1, x2, v1, v2;
	float s = 100.0;
	float y;

	/*
	ʹ��ɸѡ��������̬�ֲ�N(0,1)�������(Box-Mulles����)
	1. ����[0,1]�Ͼ����������X1,X2
	2. ����V1=2*X1-1,V2=2*X2-1,s=V1^2+V2^2
	3. ��s<=1,ת����4������ת1
	4. ����A=(-2ln(s)/s)^(1/2),y1=V1*A, y2=V2*A
	y1,y2ΪN(0,1)�������
	*/
	while ( s > 1.0 )
	{
		x1 = rand0_1();
		x2 = rand0_1();
		v1 = 2 * x1 - 1;
		v2 = 2 * x2 - 1;
		s = v1*v1 + v2*v2;
	}
	y = (float)(sqrt( -2.0 * log(s)/s ) * v1);
	/*
	���ݹ�ʽ
	z = sigma * y + u
	��y����ת����N(u,sigma)�ֲ�
	*/
	return( sigma * y + u );	
}



/*
��ʼ��ϵͳ
int x0, y0��        ��ʼ������ͼ��Ŀ����������
int Wx, Hy��        Ŀ��İ����
unsigned char * img��ͼ�����ݣ�RGB��ʽ
int W, H��          ͼ�����
*/
int Initialize( int x0, int y0, int Wx, int Hy,
			   unsigned char * img, int W, int H )
{
	int i, j;
	float rn[7];

	set_seed( 0 ); /* ʹ��ϵͳʱ����Ϊ���ӣ���������� */
	/* ϵͳ��ʼ��ʱ��Ҫ����һ��,�ҽ�����1�� */
	//NParticle = 75; /* �������Ӹ��� */
	//Pi_Thres = (float)0.90; /* ����Ȩ����ֵ */
	states = new SPACESTATE [NParticle]; /* ����״̬����Ŀռ� */
	if ( states == NULL ) return( -2 );
	weights = new float [NParticle];     /* ��������Ȩ������Ŀռ� */
	if ( weights == NULL ) return( -3 );	
	nbin = R_BIN * G_BIN * B_BIN; /* ȷ��ֱ��ͼ���� */
	ModelHist = new float [nbin]; /* ����ֱ��ͼ�ڴ� */
	if ( ModelHist == NULL ) return( -1 );

	/* ����Ŀ��ģ��ֱ��ͼ */
	CalcuColorHistogram( x0, y0, Wx, Hy, img, W, H, ModelHist, nbin );

	/* ��ʼ������״̬(��(x0,y0,1,1,Wx,Hy,0.1)Ϊ���ĳ�N(0,0.4)��̬�ֲ�) */
	states[0].xt = x0;
	states[0].yt = y0;
	states[0].v_xt = (float)0.0; // 1.0
	states[0].v_yt = (float)0.0; // 1.0
	states[0].Hxt = Wx;
	states[0].Hyt = Hy;
	states[0].at_dot = (float)0.0; // 0.1
	weights[0] = (float)(1.0/NParticle); /* 0.9; */
	for ( i = 1; i < NParticle; i++ )
	{
		for ( j = 0; j < 7; j++ ) rn[j] = randGaussian( 0, (float)0.6 ); /* ����7�������˹�ֲ����� */
		states[i].xt = (int)( states[0].xt + rn[0] * Wx );
		states[i].yt = (int)( states[0].yt + rn[1] * Hy );
		states[i].v_xt = (float)( states[0].v_xt + rn[2] * VELOCITY_DISTURB );
		states[i].v_yt = (float)( states[0].v_yt + rn[3] * VELOCITY_DISTURB );
		states[i].Hxt = (int)( states[0].Hxt + rn[4] * SCALE_DISTURB );
		states[i].Hyt = (int)( states[0].Hyt + rn[5] * SCALE_DISTURB );
		states[i].at_dot = (float)( states[0].at_dot + rn[6] * SCALE_CHANGE_D );
		/* Ȩ��ͳһΪ1/N����ÿ����������ȵĻ��� */
		weights[i] = (float)(1.0/NParticle);
	}

	return( 1 );
}



/*
�����һ���ۼƸ���c'_i
���������
float * weight��    Ϊһ����N��Ȩ�أ����ʣ�������
int N��             ����Ԫ�ظ���
���������
float * cumulateWeight�� Ϊһ����N+1���ۼ�Ȩ�ص����飬
cumulateWeight[0] = 0;
*/
void NormalizeCumulatedWeight( float * weight, float * cumulateWeight, int N )
{
	int i;

	for ( i = 0; i < N+1; i++ ) 
		cumulateWeight[i] = 0;
	for ( i = 0; i < N; i++ )
		cumulateWeight[i+1] = cumulateWeight[i] + weight[i];
	for ( i = 0; i < N+1; i++ )
		cumulateWeight[i] = cumulateWeight[i]/ cumulateWeight[N];

	return;
}

/*
�۰���ң�������NCumuWeight[N]��Ѱ��һ����С��j��ʹ��
NCumuWeight[j] <=v
float v��              һ�������������
float * NCumuWeight��  Ȩ������
int N��                ����ά��
����ֵ��
�����±����
*/
int BinearySearch( float v, float * NCumuWeight, int N )
{
	int l, r, m;

	l = 0; 	r = N-1;   /* extreme left and extreme right components' indexes */
	while ( r >= l)
	{
		m = (l+r)/2;
		if ( v >= NCumuWeight[m] && v < NCumuWeight[m+1] ) return( m );
		if ( v < NCumuWeight[m] ) r = m - 1;
		else l = m + 1;
	}
	return( 0 );
}

/*
���½�����Ҫ�Բ���
���������
float * c��          ��Ӧ����Ȩ������pi(n)
int N��              Ȩ�����顢�ز�����������Ԫ�ظ���
���������
int * ResampleIndex���ز�����������
*/
void ImportanceSampling( float * c, int * ResampleIndex, int N )
{
	float rnum, * cumulateWeight;
	int i, j;

	cumulateWeight = new float [N+1]; /* �����ۼ�Ȩ�������ڴ棬��СΪN+1 */
	NormalizeCumulatedWeight( c, cumulateWeight, N ); /* �����ۼ�Ȩ�� */
	for ( i = 0; i < N; i++ )
	{
		rnum = rand0_1();       /* �������һ��[0,1]����ȷֲ����� */ 
		j = BinearySearch( rnum, cumulateWeight, N+1 ); /* ����<=rnum����С����j */
		if ( j == N ) j--;
		ResampleIndex[i] = j;	/* �����ز����������� */		
	}

	delete[] cumulateWeight;

	return;	
}

/*
����ѡ�񣬴�N�����������и���Ȩ��������ѡ��N��
���������
SPACESTATE * state��     ԭʼ�������ϣ���N����
float * weight��         N��ԭʼ������Ӧ��Ȩ��
int N��                  ��������
���������
SPACESTATE * state��     ���¹���������
*/
void ReSelect( SPACESTATE * state, float * weight, int N )
{
	SPACESTATE * tmpState;
	int i, * rsIdx;

	tmpState = new SPACESTATE[N];
	rsIdx = new int[N];

	ImportanceSampling( weight, rsIdx, N ); /* ����Ȩ�����²��� */
	for ( i = 0; i < N; i++ )
		tmpState[i] = state[rsIdx[i]];//temStateΪ��ʱ����,����state[i]��state[rsIdx[i]]������
	for ( i = 0; i < N; i++ )
		state[i] = tmpState[i];

	delete[] tmpState;
	delete[] rsIdx;

	return;
}

/*
����������ϵͳ״̬������ȡ״̬Ԥ����
״̬����Ϊ�� S(t) = A S(t-1) + W(t-1)
W(t-1)Ϊ��˹����
���������
SPACESTATE * state��      �����״̬������
int N��                   ����״̬����
���������
SPACESTATE * state��      ���º��Ԥ��״̬������
*/
void Propagate( SPACESTATE * state, int N )
{
	int i;
	int j;
	float rn[7];

	/* ��ÿһ��״̬����state[i](��N��)���и��� */
	for ( i = 0; i < N; i++ )  /* �����ֵΪ0�������˹���� */
	{
		for ( j = 0; j < 7; j++ ) rn[j] = randGaussian( 0, (float)0.6 ); /* ����7�������˹�ֲ����� */
		state[i].xt = (int)(state[i].xt + state[i].v_xt * DELTA_T + rn[0] * state[i].Hxt + 0.5);
		state[i].yt = (int)(state[i].yt + state[i].v_yt * DELTA_T + rn[1] * state[i].Hyt + 0.5);
		state[i].v_xt = (float)(state[i].v_xt + rn[2] * VELOCITY_DISTURB);
		state[i].v_yt = (float)(state[i].v_yt + rn[3] * VELOCITY_DISTURB);
		state[i].Hxt = (int)(state[i].Hxt+state[i].Hxt*state[i].at_dot + rn[4] * SCALE_DISTURB + 0.5);
		state[i].Hyt = (int)(state[i].Hyt+state[i].Hyt*state[i].at_dot + rn[5] * SCALE_DISTURB + 0.5);
		state[i].at_dot = (float)(state[i].at_dot + rn[6] * SCALE_CHANGE_D);
		cvCircle( pTrackImg, Point(state[i].xt,state[i].yt) ,3 , CV_RGB(0,255,0),1, 8, 3 );
	}
	return;
}

/*
�۲⣬����״̬����St�е�ÿһ���������۲�ֱ��ͼ��Ȼ��
���¹�����������µ�Ȩ�ظ���
���������
SPACESTATE * state��      ״̬������
int N��                   ״̬������ά��
unsigned char * image��   ͼ�����ݣ����������ң��������µ�˳��ɨ�裬
��ɫ���д���RGB, RGB, ...						 
int W, H��                ͼ��Ŀ��͸�
float * ObjectHist��      Ŀ��ֱ��ͼ
int hbins��               Ŀ��ֱ��ͼ����
���������
float * weight��          ���º��Ȩ��
*/
void Observe( SPACESTATE * state, float * weight, int N,
			 unsigned char * image, int W, int H,
			 float * ObjectHist, int hbins )
{
	int i;
	float * ColorHist;
	float rho;

	ColorHist = new float[hbins];

	for ( i = 0; i < N; i++ )
	{
		/* (1) �����ɫֱ��ͼ�ֲ� */
		CalcuColorHistogram( state[i].xt, state[i].yt,state[i].Hxt, state[i].Hyt,
			image, W, H, ColorHist, hbins );
		/* (2) Bhattacharyyaϵ�� */
		rho = CalcuBhattacharyya( ColorHist, ObjectHist, hbins );
		/* (3) ���ݼ���õ�Bhattacharyyaϵ���������Ȩ��ֵ */
		weight[i] = CalcuWeightedPi( rho );		
	}

	delete[] ColorHist;

	return;	
}

/*
���ƣ�����Ȩ�أ�����һ��״̬����Ϊ�������
���������
SPACESTATE * state��      ״̬������
float * weight��          ��ӦȨ��
int N��                   ״̬������ά��
���������
SPACESTATE * EstState��   ���Ƴ���״̬��
*/
void Estimation( SPACESTATE * state, float * weight, int N, 
				SPACESTATE & EstState )
{
	int i;
	float at_dot, Hxt, Hyt, v_xt, v_yt, xt, yt;
	float weight_sum;

	at_dot = 0;
	Hxt = 0; 	Hyt = 0;
	v_xt = 0;	v_yt = 0;
	xt = 0;  	yt = 0;
	weight_sum = 0;
	for ( i = 0; i < N; i++ ) /* ��� */
	{
		at_dot += state[i].at_dot * weight[i];
		Hxt += state[i].Hxt * weight[i];
		Hyt += state[i].Hyt * weight[i];
		v_xt += state[i].v_xt * weight[i];
		v_yt += state[i].v_yt * weight[i];
		xt += state[i].xt * weight[i];
		yt += state[i].yt * weight[i];
		weight_sum += weight[i];
	}
	/* ��ƽ�� */
	if ( weight_sum <= 0 ) weight_sum = 1; /* ��ֹ��0����һ�㲻�ᷢ�� */
	EstState.at_dot = at_dot/weight_sum;
	EstState.Hxt = (int)(Hxt/weight_sum + 0.5 );
	EstState.Hyt = (int)(Hyt/weight_sum + 0.5 );
	EstState.v_xt = v_xt/weight_sum;
	EstState.v_yt = v_yt/weight_sum;
	EstState.xt = (int)(xt/weight_sum + 0.5 );
	EstState.yt = (int)(yt/weight_sum + 0.5 );

	return;
}


/************************************************************
ģ�͸���
���������
SPACESTATE EstState��   ״̬���Ĺ���ֵ
float * TargetHist��    Ŀ��ֱ��ͼ
int bins��              ֱ��ͼ����
float PiT��             ��ֵ��Ȩ����ֵ��
unsigned char * img��   ͼ�����ݣ�RGB��ʽ
int W, H��              ͼ����� 
�����
float * TargetHist��    ���µ�Ŀ��ֱ��ͼ
************************************************************/
# define ALPHA_COEFFICIENT      0.2     /* Ŀ��ģ�͸���Ȩ��ȡ0.1-0.3 */

int ModelUpdate( SPACESTATE EstState, float * TargetHist, int bins, float PiT,
				unsigned char * img, int W, int H )
{
	float * EstHist, Bha, Pi_E;
	int i, rvalue = -1;

	EstHist = new float [bins];

	/* (1)�ڹ���ֵ������Ŀ��ֱ��ͼ */
	CalcuColorHistogram( EstState.xt, EstState.yt, EstState.Hxt, 
		EstState.Hyt, img, W, H, EstHist, bins );
	/* (2)����Bhattacharyyaϵ�� */
	Bha  = CalcuBhattacharyya( EstHist, TargetHist, bins );
	/* (3)�������Ȩ�� */
	Pi_E = CalcuWeightedPi( Bha );

	if ( Pi_E > PiT ) 
	{
		for ( i = 0; i < bins; i++ )
		{
			TargetHist[i] = (float)((1.0 - ALPHA_COEFFICIENT) * TargetHist[i]
			+ ALPHA_COEFFICIENT * EstHist[i]);
		}
		rvalue = 1;
	}

	delete[] EstHist;

	return( rvalue );
}

/*
ϵͳ���
*/
void ClearAll()
{
	if ( ModelHist != NULL ) delete [] ModelHist;
	if ( states != NULL ) delete [] states;
	if ( weights != NULL ) delete [] weights;

	return;
}


int ColorParticleTracking( unsigned char * image, int W, int H, 
						  int & xc, int & yc, int & Wx_h, int & Hy_h,
						  float & max_weight )
{
	SPACESTATE EState;
	int i;

	ReSelect( states, weights, NParticle );
	/* ����������״̬���̣���״̬��������Ԥ�� */
	Propagate( states, NParticle );
	/* �۲⣺��״̬�����и��� */
	Observe( states, weights, NParticle, image, W, H,
		ModelHist, nbin );
	/* ���ƣ���״̬�����й��ƣ���ȡλ���� */
	Estimation( states, weights, NParticle, EState );
	xc = EState.xt;
	yc = EState.yt;
	Wx_h = EState.Hxt;
	Hy_h = EState.Hyt;
	/* ģ�͸��� */
	ModelUpdate( EState, ModelHist, nbin, Pi_Thres,	image, W, H );

	/* �������Ȩ��ֵ */
	max_weight = weights[0];
	for ( i = 1; i < NParticle; i++ )
		max_weight = max_weight < weights[i] ? weights[i] : max_weight;
	/* ���кϷ��Լ��飬���Ϸ�����-1 */
	if ( xc < 0 || yc < 0 || xc >= W || yc >= H ||
		Wx_h <= 0 || Hy_h <= 0 ) return( -1 );
	else 
		return( 1 );		
}



//��iplimage ת��img ������
void IplToImge(IplImage* src, int w,int h)
{
	int i,j;
	for ( j = 0; j < h; j++ ) // ת������ͼ��
		for ( i = 0; i < w; i++ )
		{
			img[ ( j*w+i )*3 ] = R(src,i,j);
			img[ ( j*w+i )*3+1 ] = G(src,i,j);
			img[ ( j*w+i )*3+2 ] = B(src,i,j);
		}
}


void mouseHandler( int event , int x, int y, int flags, void* param)
{
	int centerx,centery;

	if( bSelectObject)
	{
		selection.x = MIN(x,origin.x);
		selection.y = MIN(y,origin.y);
		selection.width = abs( x - origin.x);
		selection.height = abs( y - origin.y);
	}

	switch(event)
	{
	case CV_EVENT_LBUTTONDOWN://�������ʱ
		origin  = Point(x,y);
		selection = Rect(x,y,0,0);
		bSelectObject = 1;
		pause = false;
		break;
	case CV_EVENT_LBUTTONUP://�ͷ����ʱ
		bSelectObject = 0;
		centerx = selection.x + selection.width;
		centery = selection.y + selection.height;
		WidIn = selection.width / 2;
		HeiIn = selection.height / 2;
		Initialize( centerx, centery, WidIn, HeiIn, img, Wid, Hei );
		track = true;
		//pause = true;
		break;
	}
}

void mouseHandler1(int event, int x, int y, int flags, void* param)//������Ҫע�⵽Ҫ�ٴε���cvShowImage��������ʾ����
{
	CvMemStorage* storage = cvCreateMemStorage(0);
	CvSeq * contours;
	IplImage* pFrontImg1 = 0;
	int centerX,centerY;
	int delt = 10;
	pFrontImg1=cvCloneImage(pFrontImg);//����ҲҪע�⵽����� cvShowImage("foreground",pFrontImg1)����pFrontImg��Ч���������¶��岢����
	switch(event){
	  case CV_EVENT_LBUTTONDOWN:	
		  //Ѱ������
		  cout<<"********"<<endl;
		  if(1)
		  {
			  //��pFrontImg��ѡȡ��ͨ��
			  cvFindContours(pFrontImg,storage,&contours,sizeof(CvContour),CV_RETR_EXTERNAL,
				  CV_CHAIN_APPROX_SIMPLE);

			  //��ԭ�����л���Ŀ����������Ӿ���
			  for (;contours;contours = contours->h_next)   
			  {
				  CvRect r = ((CvContour*)contours)->rect;
				  if(x>r.x&&x<(r.x+r.width)&&y>r.y&&r.y<(r.y+r.height))
				  {
					  if (r.height*r.width>CONTOUR_MIN_AREA && r.height*r.width<CONTOUR_MAX_AREA)
					  {
						  centerX = r.x+r.width/2;//�õ�Ŀ�����ĵ�
						  centerY = r.y+r.height/2;
						  WidIn = r.width/2;//�õ�Ŀ��������
						  HeiIn = r.height/2;
						  xin = centerX;
						  yin = centerY;
						  cvRectangle(pFrontImg1,cvPoint(r.x,r.y),cvPoint(r.x+r.width,r.y+r.height),cvScalar(255,0,0),2,8,0);	
						  Initialize( centerX, centerY, WidIn, HeiIn, img, Wid, Hei );
						  cout<<"star"<<endl;
						  track = true;//���и���
						  cvShowImage("foreground",pFrontImg1);
						  return;

					  }
				  }

			  }
		  }

		  break;
	}
}


void main(int argc, char *argv[])
{
	int FrameNum=0;  //֡��
	int k=0;
	CvCapture * capture = 0;

	/*if( argc == 1 || (argc == 2 && strlen(argv[1]) == 1 && isdigit(argv[1][0])))
		capture = cvCaptureFromCAM( argc == 2 ? argv[1][0] - '0' : 0 );
	else if( argc == 2 )*/
		capture = cvCaptureFromAVI( /*argv[1]*/"../13.avi");

	IplImage* frame[Num]; //�������ͼ��
	int i,j;
	uchar key = false;      //����������ͣ
	float rho_v;//��ʾ���ƶ�
	float max_weight;

	int sum=0;    //���������ͼ��֡����ֵ
	for (i=0;i<Num;i++)
	{
		frame[i]=NULL;
	}

	IplImage *curFrameGray=NULL;
	IplImage *frameGray=NULL;

	CvMat *Mat_D,*Mat_F;   //��̬������֡������
	int row ,col;
	cvNamedWindow("video",1);
	//cvNamedWindow("background",1); 
	//cvNamedWindow("foreground",1);   
	cvNamedWindow("tracking",1);
	int star = 0;
	while (capture)
	{
		curframe=cvQueryFrame(capture); //ץȡһ֡
		if(!star)//��ʼ��
		{
			curFrameGray=cvCreateImage(cvGetSize(curframe),IPL_DEPTH_8U,1);
			frameGray=cvCreateImage(cvGetSize(curframe),IPL_DEPTH_8U,1);
			pBackImg=cvCreateImage(cvGetSize(curframe),IPL_DEPTH_8U,1);
			pFrontImg=cvCreateImage(cvGetSize(curframe),IPL_DEPTH_8U,1);
				//		pTrackImg = cvCreateImage(cvGetSize(curframe),IPL_DEPTH_8U,3);

			cvSetZero(pFrontImg);  
			cvCvtColor(curframe,pBackImg,CV_RGB2GRAY);

			row=curframe->height;
			col=curframe->width;
			Mat_D=cvCreateMat(row,col,CV_32FC1);
			cvSetZero(Mat_D);  
			Mat_F=cvCreateMat(row,col,CV_32FC1);
			cvSetZero(Mat_F);
			Wid = curframe->width;
			Hei = curframe->height; 
			img = new unsigned char [Wid * Hei * 3];
			star = 1;
		}
		pTrackImg = cvCloneImage(curframe);
		IplToImge(curframe,Wid,Hei);//��iplimageת����img������
		//pTrackImg = cvCloneImage(curframe);
		//cvCvtColor(curframe,curFrameGray,CV_RGB2GRAY);
		
		if(track)
		{
			/* ����һ֡ */
			rho_v = ColorParticleTracking( img, Wid, Hei, xout, yout, WidOut, HeiOut, max_weight );
			/* ����: ��λ��Ϊ���� */
			if ( rho_v > 0 && max_weight > 0.0001 )  /* �ж��Ƿ�Ŀ�궪ʧ */
			{
					cvRectangle(pTrackImg,cvPoint(xout - WidOut,yout - HeiOut),cvPoint(xout+WidOut,yout+HeiOut),cvScalar(255,0,0),2,8,0);
					xin = xout; yin = yout;
					WidIn = WidOut; HeiIn = HeiOut;
			}
			else
			{
				cout<<"target lost"<<endl;
			}
		}

		cvShowImage("video",curframe);
		cvShowImage("tracking",pTrackImg);
		cvSetMouseCallback("video",mouseHandler,0);
		cvWaitKey(10);
	

	}
	cvReleaseImage(&curFrameGray);
	cvReleaseImage(&frameGray);
	cvReleaseImage(&pBackImg);
	cvReleaseImage(&pFrontImg);
	cvDestroyAllWindows();
	ClearAll();
	}
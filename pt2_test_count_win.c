/************************************************************************

   PicoHarp 300			File Access Demo in C

  Demo access to binary PicoHarp 300 T2 Mode Files (*.pt2)
  for file format version 2.0 only
  Read a PicoHarp data file and dump the contents in ASCII
  Michael Wahl, PicoQuant GmbH, September 2006, updated May 2007

  Tested with the following compilers:

  - MinGW 2.0.0-3 (free compiler for Win 32 bit)
  - MS Visual C++ 4.0/5.0/6.0 (Win 32 bit)
  - Borland C++ 5.5 (Win 32 bit)

  It should work with most others.
  Observe the 4-byte structure alignment!

  This is demo code. Use at your own risk. No warranties.


  Note that markers have a lower time resolution and may therefore appear 
  in the file slightly out of order with respect to regular event records.
  This is by design. Markers are designed only for relatively coarse 
  synchronization requirements such as image scanning. 


************************************************************************/


#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#define DISPCURVES 8
#define RESOL 4E-12 //4ps
#define T2WRAPAROUND 210698240 
#define MEASMODE_T2 2 


/*
The following structures are used to hold the file data
They directly reflect the file structure.
The data types used here to match the file structure are correct
for the tested compilers.
They may have to be changed for other compilers.
*/


#pragma pack(4) //structure alignment to 4 byte boundaries

/* These are substructures used below */

typedef struct{ float Start;
                float Step;
				float End;  } tParamStruct;

typedef struct{ int MapTo;
				int Show; } tCurveMapping;

/* The following represents the readable ASCII file header portion */

struct {		char Ident[16];				//"PicoHarp 300"
				char FormatVersion[6];		//file format version
				char CreatorName[18];		//name of creating software
				char CreatorVersion[12];	//version of creating software
				char FileTime[18];
				char CRLF[2];
				char CommentField[256]; } TxtHdr;

/* The following is binary file header information */

struct {		int Curves;
				int BitsPerRecord;
				int RoutingChannels;
				int NumberOfBoards;
				int ActiveCurve;
				int MeasMode;
				int SubMode;
				int RangeNo;
				int Offset;
				int Tacq;				// in ms
				int StopAt;
				int StopOnOvfl;
				int Restart;
				int DispLinLog;
				int DispTimeFrom;		// 1ns steps
				int DispTimeTo;
				int DispCountsFrom;
				int DispCountsTo;
				tCurveMapping DispCurves[DISPCURVES];	
				tParamStruct Params[3];
				int RepeatMode;
				int RepeatsPerCurve;
				int RepeatTime;
				int RepeatWaitTime;
				char ScriptName[20];	} BinHdr;

/* The next is a board specific header */

struct {		
				char HardwareIdent[16]; 
				char HardwareVersion[8]; 
				int HardwareSerial; 
				int SyncDivider;
				int CFDZeroCross0;
				int CFDLevel0;
				int CFDZeroCross1;
				int CFDLevel1;
				float Resolution;
				//below is new in format version 2.0
				int RouterModelCode;
				int RouterEnabled;
				int RtChan1_InputType; 
				int RtChan1_InputLevel;
				int RtChan1_InputEdge;
				int RtChan1_CFDPresent;
				int RtChan1_CFDLevel;
				int RtChan1_CFDZeroCross;
				int RtChan2_InputType; 
				int RtChan2_InputLevel;
				int RtChan2_InputEdge;
				int RtChan2_CFDPresent;
				int RtChan2_CFDLevel;
				int RtChan2_CFDZeroCross;
				int RtChan3_InputType; 
				int RtChan3_InputLevel;
				int RtChan3_InputEdge;
				int RtChan3_CFDPresent;
				int RtChan3_CFDLevel;
				int RtChan3_CFDZeroCross;
				int RtChan4_InputType; 
				int RtChan4_InputLevel;
				int RtChan4_InputEdge;
				int RtChan4_CFDPresent;
				int RtChan4_CFDLevel;
				int RtChan4_CFDZeroCross;		} BoardHdr;


/* The next is a TTTR mode specific header */

struct {
				int ExtDevices;
				int Reserved1;
				int Reserved2;			
				int CntRate0;
				int CntRate1;
				int StopAfter;
				int StopReason;
				int Records;
				int ImgHdrSize;		} TTTRHdr;


/*The following data records appear for each T2 mode event*/


 union	{ 
				 unsigned int allbits;
				 struct
				 {	
					unsigned time		:28; 		
					unsigned channel	:4;
				 } bits;

		} Record;

int main(int argc, char* argv[])
{

 FILE *fpin,*fpout,*fppars; 		/* input/output and buffer file pointers */
 int i,j,result;
 long int cur;
 int64_t time=0, ofltime0=0, ofltime=0, ref_time=0, rupt_time=0, dt=0;
 unsigned int markers;
 
 //my pars
 int progressNumMax = 75;
 int ascii=219;
 
 clock_t start, end;
 
 //histogram part and default values
 int k,divider=100,max,weight=1;
 int width_ns=50;	// ns
 double width;
 	  width = (double)width_ns*1e-9;
 int Num=200, center;	// #histogram blogs
     center=((int)Num/2);
 int *height;
 
 	
 	
 //including the parameters	
 if((fppars=fopen("parameters","r"))==NULL)
 {
  printf("\ncannot open parameters file\n");
  goto ex;
 }
 
 printf("\n\nRead parameters:\n");
 fscanf(fppars,"blog width (ns) : ");
 fscanf(fppars,"%d",&width_ns);
 	width = (double)width_ns*1e-9;
 	printf("\t width_ns : %d\n",width_ns);
 fscanf(fppars,"\nnumber of blogs : ");
 fscanf(fppars,"%d",&Num);
 	printf("\t Num : %d\n",Num);
    center=((int)Num/2);
 fscanf(fppars,"\ndivider of Records : ");
 fscanf(fppars,"%d",&divider);
 	printf("\t divider of Records : %d\n",divider);
    max=((int)(TTTRHdr.Records/divider));
 fscanf(fppars,"\nweigth between 1 and 0 (#1/#0) : ");
 fscanf(fppars,"%d",&weight);
    printf("\t weigth : %d",weight);
 fclose(fppars);
 
 height = (int*)calloc(sizeof(int),Num);
 for(i=0;i<Num;i++)
 	height[i]=0;
 
 
 //printf("\nPicoHarp T2 Mode File Demo");
 printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~");

 if(argc!=3)
 {
  printf("\nUsage: pt2demo infile outfile");
  printf("\ninfile is a binary PicoHarp 300 T2 mode file (*.pt2)");
  printf("\noutfile will be ASCII");
  //printf("\nNote that this is only a demo. Routinely converting T2 data");
  printf("\nto ASCII is inefficient and therefore discouraged.");
  goto ex;
 }

 if((fpin=fopen(argv[1],"rb"))==NULL)
 {
  printf("\ncannot open input file\n");
  goto ex;
 }

 if((fpout=fopen(argv[2],"w"))==NULL)
  {
   printf("\ncannot open output file\n");
   goto ex;
  }

 //skip_headersection
 fseek(fpin,sizeof(TxtHdr),SEEK_SET);
 //result = fread( &TxtHdr, 1, sizeof(TxtHdr) ,fpin);
 fseek(fpin,sizeof(BinHdr),SEEK_CUR);
 //result = fread( &BinHdr, 1, sizeof(BinHdr) ,fpin);
 fseek(fpin,sizeof(BoardHdr),SEEK_CUR);
 //result = fread( &BoardHdr, 1, sizeof(BoardHdr) ,fpin);
 //fseek(fpin,sizeof(TTTRHdr),SEEK_CUR);
 result = fread( &TTTRHdr, 1, sizeof(TTTRHdr) ,fpin);
 fseek(fpin,TTTRHdr.ImgHdrSize*4,SEEK_CUR);


/* Now read and interpret the TTTR records */           

 printf("\nprocessing..\n");
 printf("[");for(k=0;k<progressNumMax+1;k++) printf(".");printf("]"); // for progress bar
 for(k=0;k<progressNumMax+2;k++) printf("\b");
 
 //start histogram method for correlation profiling
 
 
 
 //fprintf(fpout,"\nrecord# chan   rawtime      time/4ps   time/sec\n");

 //printf("%d\n",TTTRHdr.Records);
 max=((int)(TTTRHdr.Records/divider));

 start = clock();
 
 for(i=0;i<(max);i++)
 {
	//progress report
    //if(i%((int)(max/10))==0) printf("\n...%2d",i/(max/100)); //old progress report
	if(i%(max/progressNumMax)==0) printf("%c",ascii);
	
	
	// END progress report
    
    result = fread( &Record, 1, sizeof(Record) ,fpin);
	if (result!= sizeof(Record))
	{
		printf("\n\nUnexpected end of input file!");
		break;
	}

	//fprintf(fpout,"%7u %08x ",i,Record.allbits);

	if(Record.bits.channel==0xF) //this means we have a special record
	{
		//in a special record the lower 4 bits of time are marker bits
		markers=Record.bits.time&0xF;
		if(markers==0) //this means we have an overflow record
		{
			ofltime0 += T2WRAPAROUND; // unwrap the time tag overflow 
			//fprintf(fpout," ofl\n");
		}
		else //a marker
		{
			//Strictly, in case of a marker, the lower 4 bits of time are invalid
			//because they carry the marker bits. So one could zero them out. 
			//However, the marker resolution is only a few tens of nanoseconds anyway,
			//so we can just ignore the few picoseconds of error.
			time = ofltime0 + Record.bits.time;

			//fprintf(fpout,"MA%1u\n",markers;
		}
		continue;		
	}		

	/*if((int)Record.bits.channel>BinHdr.RoutingChannels) //Should not occur 
	{
		printf(" Illegal Chan: #%1d %1u\n",i,Record.bits.channel);
		fprintf(fpout," illegal chan.\n");
		continue;
	}*/

	time = ofltime0 + Record.bits.time;

	if(Record.bits.channel==0) 
	{
		ref_time=time;
		rupt_time=0;
		ofltime=0;
				
		//to scan from initial, keep the current position
		cur=ftell(fpin);
		//skip header
		result=sizeof(TxtHdr)+sizeof(BinHdr)+sizeof(BoardHdr)+sizeof(TTTRHdr)+TTTRHdr.ImgHdrSize*4;
		//printf("result = %d\n",result);
		fseek(fpin,result,SEEK_SET);
		//printf("\nftell1 = %ld",ftell(fpin));

		for(k=0;k<(weight*max);k++)
		{
			//printf("k = %d\n",k);
			result = fread( &Record, 1, sizeof(Record) ,fpin);
			//printf("%ld\n",ftell(fpin));
			if (result!= sizeof(Record))
			{
				printf("\ni = %d k =  %d",i,k);
				printf("\nftell2 = %ld",ftell(fpin));
				printf("\nUnexpected end of buffer input file!");
				break;
			}
			
			//printf("check point = 1\n");

			if(Record.bits.channel==0xF) //this means we have a special record
			{		
				//in a special record the lower 4 bits of time are marker bits
				markers=Record.bits.time&0xF;
				if(markers==0) //this means we have an overflow record
				{
					ofltime += T2WRAPAROUND; // unwrap the time tag overflow 
					//fprintf(fpout," ofl\n");
				}
				else //a marker
					rupt_time = ofltime + Record.bits.time;
				continue;		
			}			

			rupt_time = ofltime + Record.bits.time;
			
			//printf("check point = 2\n");

			if(Record.bits.channel==0)
			{
				//printf("check point = if chan0\n");
				continue;
			}
			else if(Record.bits.channel==1)
			{
				//printf("check point = else if chan1\n");
				dt = rupt_time - ref_time;
				if(dt < -0.5*Num*width_ns*250) 
				{
					//printf("dt = %lld\n",dt);
					continue;
				}
				if(dt > 0.5*Num*width_ns*250) break;
				//printf("%lld\n",dt);
				
				/* chx old version for make hist
                for(j=0;j<Num;j++)
				{
					if ((((double)dt*RESOL) > (((double)(j-center))*width)) && (((double)dt*RESOL) < (((double)j+1-center)*width)))
						height[j]++;
					else
						continue; 
				}*/
				
				j = (int)floor((dt*RESOL)/width);
                height[j+center]++;
				
			}
			else
			{
				//printf("check point = else\n");
				continue;
			}
		}		

				
		//move to marked position
		fseek(fpin,cur,SEEK_SET);

	}
	if(Record.bits.channel>=1)
		continue;


	/*fprintf(fpout,"  %1u %12u %12lld %14.12lf\n",Record.bits.channel,Record.bits.time,time,(double)(time*RESOL));*/
 }
 printf("]\n\n"); // END progress bar

//print section
 fprintf(fpout,"parameters\n\tblog width = %14.12lf (s)\n\tNumber of blogs = %d\n",width,Num);
 for(k=0;k<Num;k++)
 {
 	fprintf(fpout,"\n%14.12lf\t%12d",((double)(k-center)*width),height[k]);
 }

close:
 fclose(fpin);
 fclose(fpout);
ex:
 end = clock();
 printf("time usage = %.2f(sec), %.2f(min)\n\n",(end-start)/(double)CLOCKS_PER_SEC,(end-start)/(double)CLOCKS_PER_SEC/60.);
 //printf("\npress return to exit");
 //getchar();
 return(0);
}

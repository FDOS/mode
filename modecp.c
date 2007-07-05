/* This code can be used under the terms of  GNU Public License */
/* (GPL) version 2 or later.   Written for FreeDOS MODE by Eric */
/* Auer (eric@coli.uni-sb.de) (c) 2003-2004. GPL -> www.gnu.org */

/* MODE command: CODEPAGE related parts */

#include "mode.h"

#include <io.h>
#include <fcntl.h>
#if EXEFILE
#else
#include <alloc.h>
#endif

#if HAVE_TASM
#define CF 1	/* Carry flag */
#else
static union REGS r;
#endif

#define BYTE unsigned char

			/* action to be performed */
#define MA_Query    1   /* MODE detects a problem from DISPLAY.SYS */
#define MA_Prepare  2
#define MA_Select   3
#define MA_MODE   100   /* MODE detected a problem */


#define UPXoverhead  320	/* actually even less: UPX stub has  */
	/* 2 parts - 1. relocate 2. decompress - and uses almost no  */
	/* stack. We do not even use stack inside the buffer at all, */
	/* makes no difference for the UPX stub. Actual overhead is  */
	/* therefore only part 2 of the stub plus no extra stack...  */
#define MaxBufSize   (58880U + UPXoverhead)
	/* "CPX" support decompresses data  in place  in the buffer. */

/*////////// V A R I A B L E S  ////////////////////////////////////*/


	unsigned int table[20];	 /* table to allocate current status of DISPLAY */
#if EXEFILE
#error Compiling to EXE will break CPX support unless buf is paragraph aligned!
	unsigned char buf[MaxBufSize];  /* was 9728 with raw files */
				/* CPI are header plus 5 or 6 raw font sets */
#else
	unsigned char far * buf; /* made pointer far */
#endif


/*////////// F U N C T I O N S  ////////////////////////////////////*/



extern int cpx2cpi(void far * buffer); /* returns 0 plain 1 upxed -1 error */
	/* this part is written in NASM Assembly language, compile */
	/* it with NASM -fobj -o cpitrick.obj cpitrick.asm */



void deviceError ( BYTE action, BYTE errorcode )
{
	if (action==MA_Query)
	{
		switch (errorcode)	{
			case 26:  puts ("MODE: Active codepage was never set");
				    break;
			case 27:  puts ("MODE: Device read codepage error");
				    break;
			default:  puts ("MODE: Unknown codepage read error");
		}
	}
	else if (action==MA_Prepare)
	{
		switch (errorcode)	{
			case 27:  puts ("MODE: Codepage was not found in CPI file");
				    break;
			case 29:  puts ("MODE: Could not get codepage info from file (or XMS troubles)");
				    break;
			case 31:  puts ("MODE: specified CPI file is damaged (or XMS troubles)");
				    break;
			default:  /* and case 29 (Aitor: unknown error reason?) */
				    puts ("MODE: Unknown codepage prepare error");
		}
	}
	else if (action==MA_Select)
	{
		switch (errorcode)	{
			case 26:  puts ("MODE: Specified codepage was not found in file");
				    break;
			case 27:  puts ("MODE: KEYB failed to change codepage");
				    break;
			case 29:  puts ("MODE: Device select codepage failed (or XMS troubles)");
				    break;
			case 31:  puts ("MODE: Device select codepage error");
				    break;
			default:  puts ("MODE: Unknown select codepage error");
		}
	}
	else if (action==MA_MODE)
	{
		switch (errorcode)	{
			case 100:  puts ("MODE: INTERNAL ERROR: size for prepared table too small");
				     break;
			case 101:  puts ("MODE: No compatible FreeDOS DISPLAY driver loaded.");
				   puts ("Syntax example: ... DISPLAY CON=(VGA,437,1)");
				     break;
			case 102:  puts ("MODE: File not found");
				     break;
			case 103:  puts ("MODE: Error specifying codepage / buffer");
				     break;
			case 104:  puts ("MODE: Error: selected buffer not present");
				     break;
			default:   puts ("MODE: Unknown MODE error");
		}
	}
	else puts ("MODE: Unknown agent error");

	exit (errorcode);
}


unsigned int GetCodePage ( void )
{
#if HAVE_TASM
	unsigned int cp;
	asm {  MOV AX, 0xAD02
	       MOV BX, 0xfffe
		 INT 0x2F
		 MOV cp, BX
	}
	return cp;
#else
	r.x.ax = 0xad02;
	r.x.bx = 0xfffe;
	int86(0x2f, &r, &r);
	return r.x.bx;
#endif
}


void FillTableData ( void )
{
#if HAVE_TASM
	unsigned int s,o;
#else
	struct SREGS sregs;
#endif
	unsigned char i;

	for (i=0; i<20; i++) table[i]=0;

#if HAVE_TASM
	s = FP_SEG (table);
	o = FP_OFF (table);
	asm {	MOV AX, 0xAD03
		MOV CX, 25
		MOV ES, s
		MOV DI, o
		INT 0x2F
		PUSHF
		POP AX
		MOV i,AL
		}
	if (i & CF)
		deviceError (MA_MODE, 100);
#else
	r.x.ax = 0xad03;
	r.x.cx = 25;
	r.x.di = FP_OFF(table);
	sregs.es = FP_SEG(table);
	int86x(0x2f, &r, &r, &sregs);
	if (r.x.cflag)
		deviceError (MA_MODE, 100);
#endif
	if (! (table[2] | table[0]) )
		deviceError (MA_Query, 27);
}


unsigned int ShowStatus ( void )
{
	unsigned int cp=0;
	unsigned char i;

	cp = GetCodePage();

	if (cp == 0xfffe) {
	  puts ("CON codepage support (FreeDOS DISPLAY) not loaded.");
	  return 0; /* no DISPLAY (codepage support) loaded, simple... */
	}

	printf ("CODEPAGE status for device CON: ");

        printf("Active codepage for device CON ");
	if (cp != 0xFFFF)
		printf ("is %u.\r\n", cp);
	else
		puts ("[no set yet].");	/* puts implies \r\n */

	FillTableData ();
	printf ("  Subfonts: %u  Hardware codepages: ",table[1]);
	for (i=1;i<=table[2];i++)
	  printf ("%u%s", table[2+i], (i<table[2]) ? ", " : "");
	printf ("  Prepared codepages: ");
	for (i=1;i<=table[0];i++)
          printf ("%u%s", table[2+i+table[2]], (i<table[0]) ? ", " : "");
	printf ("\r\n");
	/* puts   ("\r\nMODE status code page function completed."); */
	return (cp);
}



unsigned int CodePageSelect ( unsigned int cp )
{
	unsigned int success;

#if HAVE_TASM
	asm { MOV AX, 0xAD01
		MOV BX, cp
		INT 0x2F
		MOV success, AX
	}
#else
	r.x.ax = 0xad01;
	r.x.bx = cp;
	int86(0x2f, &r, &r);
	success = r.x.ax;
#endif
        if (success == 0xad01) {
          puts("FreeDOS DISPLAY not loaded - select codepage not possible.");
          return -1;
        }
	printf ("MODE select codepage %u function %s\r\n", cp,
	(success) ? "completed" : "failed");

	if (!success) {
#if HAVE_TASM
		asm { MOV AX, 0xAD05
			XOR BX,BX
			INT 0x2F
			MOV success, BX
		}
#else
		r.x.ax = 0xad05;
		r.x.bx = 0;
		int86(0x2f, &r, &r);
		success = r.x.bx;
#endif
		deviceError (MA_Select, success);
	}

	return (success);
}



unsigned int CheckFDDisplayVer ( void )
{
	unsigned char Ral;	/* DISPLAY before 0.10b erroneously used AH */
	unsigned int  Rbx;

#if HAVE_TASM
	asm { MOV AX, 0xAD00
		INT 0x2F
		MOV Ral, AL
		MOV Rbx, BX
	}
#else
	r.x.ax = 0xad00;
	int86(0x2f, &r, &r);
	Ral = r.h.al;		/* 0.10b+ */
	if (r.h.ah == 0xff) Ral = 0xff;	/* compatibility for DISPLAY 0.10 */
	Rbx = r.x.bx;
#endif

	return ( (Ral==0xFF) && (Rbx>=10) && (Rbx<=11) );
					/* version 0.10-0.11 only! */
}					/* (0.11 allowed 20aug2004) */


unsigned int  ReadNumber (char *line, unsigned int *lineptr )
{
	char     cvtstr[8];
	unsigned int i=0;

	while ( (line[*lineptr]>='0') && (line[*lineptr]<='9'))
		cvtstr[i++] = line[(*lineptr)++];
	cvtstr[i] = 0;

	return (  (unsigned int) (atoi (cvtstr)) );
}



/* returns buffer to fill, starts on 0 */
unsigned int GetPosition (char *s, unsigned int *cp)
{
	unsigned int pos = 0;
	unsigned int i = 1;

	if (s[0] != '(')
		deviceError (MA_MODE, 103);

	while (s[i]==',')
	{	pos++;  i++; }

	*cp = ReadNumber (s, &i);

	while (s[i]==',') i++;



	if (s[i++] != ')')
		deviceError (MA_MODE, 103);

	if (s[i])
		deviceError (MA_MODE, 102);

	return pos;
}



/* if an error is detected, deviceError calls exit() directly! */
unsigned int CodePagePrepare (char *bufferpos, char *filename)
{
	unsigned int cpf;
	unsigned int  cp, pos;
	unsigned int  n;
	unsigned long m; /* filelen is (FAT16: signed) long! */
	unsigned int  s,o;
#if HAVE_TASM
	unsigned int  f;
#else
	struct SREGS sregs;
#endif

	/** Determine that it is FreeDOS DISPLAY **/
	if (!CheckFDDisplayVer())
		deviceError (MA_MODE, 101); /* install check failed */

	/** Parse the number and position **/
	pos = GetPosition (bufferpos, &cp);

	FillTableData ();

	if (pos > table[0] )
		deviceError (MA_MODE, 104);

	/** Check that the file exists **/
	if ((cpf = open(filename, O_RDONLY | O_BINARY)) == 0xffffU)
		deviceError (MA_MODE, 102);

	/** Check that has correct size, warn otherwise **/
#if 0	/* Files can be 58880 or 49100 bytes for CPI, and will be */
	/* something like 6-7k for UPXed CPI (CPX) file format.
	if ( filelength(cpf) != 58880U)
		puts ("Warning: CPI file is not 58880 bytes, DISPLAY may fail.");
#endif

#if EXEFILE
	/* could clean buffer here */
#else
	buf = farmalloc(MaxBufSize+15); /* changed malloc to farmalloc */
	if (buf == NULL) {
	   puts ("Out of memory!");
	   exit(1);
	}

	/* for UPXed CPI file (CPX file) support, buffer must be aligned */
	while (FP_OFF(buf) % 16)	/* "move" buf to next paragraph */
		buf++;
	while (FP_OFF(buf)) {		/* normalize buf to "someseg:0" */
		buf = MK_FP(FP_SEG(buf)+1, FP_OFF(buf)-16);
	}

	/* we do NOT free malloced buf later - happens on exit anyway. */
#endif

	/** Read the buffer **/
#if EXEFILE
	n = read(cpf, buf, MaxBufSize);
#else
	/* if memory model is TINY (.com file), we need our */
	/* OWN read( handle, FAR buffer pointer, size ) ... */
        sregs.ds = FP_SEG(buf);
        r.x.dx = FP_OFF(buf);
        r.h.ah = 0x3f;	/* read from file handle */
        r.x.cx = MaxBufSize - UPXoverhead;
        r.x.bx = cpf;	/* handle */
#if 0
	printf("%u:%u size=%u handle=%u\n", sregs.ds, r.x.dx, r.x.cx, r.x.bx); 
#endif
	int86x(0x21, &r, &r, &sregs);
	if ( r.x.cflag ) {
		printf("CPI file read error, code %u.\n", r.x.ax);
		r.x.ax = 0;	/* read no bytes, error */
        }
        n = r.x.ax;	/* number of successfully read bytes */
#endif
	m = filelength (cpf);
	close (cpf);
	if ( n != m ) {
		printf("Read only %u bytes of CPI file with %lu bytes size.\n",
			n, m);
		deviceError (MA_Prepare, 29);
	}
	/* close (cpf); */


	m = cpx2cpi(buf); /* ret: 0 plain 1 upxed -1 error */ /* new 29apr2004 */

	if (m == -1) {
		printf("This is not a CPI or CPX (UPXed CPI) file.\n");
		deviceError (MA_Prepare, 29);
	} else if (m == 1) {
		printf("Uncompressing and loading UPXed CPI (CPX) file :-).\n");
	} else {
		printf("Loading CPI file.\n");
#ifdef VMODE
		printf("Hint: rename CPI to COM, compress with http://UPX.sf.net, rename to CPX.\n");
#endif
	}


	/** Send the buffer **/
	s = FP_SEG(buf);
	o = FP_OFF(buf);
	s = s + ((o & 0xFFF0) >> 4);	/* normalize a bit */
	o &= 0x000F;

#if HAVE_TASM
	asm {   PUSH DS

		  MOV AX, 0xAD0E
		  MOV BX, cp
		  MOV DX, pos
		  MOV DS, s
		  MOV SI, o
		  INT 0x2F
		  PUSHF
		  POP AX

		POP DS

		  MOV f, AX
	}
	/** Check the result **/
	if ( f & CF )
#else
	r.x.ax = 0xad0e;
	r.x.bx = cp;
	r.x.dx = pos;
	r.x.si = o;
	sregs.ds = s;
	int86x(0x2f, &r, &r, &sregs);
	if ( r.x.cflag ) /* no "no DISPLAY at all" check needed here */
#endif
	{
		printf ("Codepage %u prepare failed.\r\n", cp);
#if HAVE_TASM
		asm { MOV AX, 0xAD05
			XOR BX,BX
			INT 0x2F
			MOV cpf, BX
		}
#else
		r.x.ax = 0xad05;
		r.x.bx = 0;
		int86(0x2f, &r, &r);
		cpf = r.x.bx;
#endif
		deviceError (MA_Prepare, cpf);
	}

#ifdef DMODE
	printf ("MODE prepare codepage %u function completed\n", cp);
#endif
	return 0; /* if not completed, exit() got called earlier... */
}


#if 0
 REFRESH/REF -> x = GetCodePage(), if != 0xffff, do CodePageSelect(x), else fail.
 PREPARE/PREP x y -> CodePagePrepare (x, y), both strings.
 SELECT/SP x -> CodePageSelect(codpn)
 --- -> ShowStatus()
   puts ("- To prepare a buffer, set the number in brackets, with commas to");
   puts ("  indicate the buffer position. E.g.  (,,866) prepares cp866 in the");
   puts ("  third buffer (only ONE buffer at a time)");
#endif


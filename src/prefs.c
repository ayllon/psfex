/*
*				prefs.c
*
* Functions related to run-time configurations.
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*	This file part of:	PSFEx
*
*	Copyright:		(C) 1997-2012 Emmanuel Bertin -- IAP/CNRS/UPMC
*
*	License:		GNU General Public License
*
*	PSFEx is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
* 	(at your option) any later version.
*	PSFEx is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*	You should have received a copy of the GNU General Public License
*	along with PSFEx.  If not, see <http://www.gnu.org/licenses/>.
*
*	Last modified:		11/07/2012
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include        "config.h"
#endif

#include	<ctype.h>
#include	<math.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include        <unistd.h>

#if defined(USE_THREADS) \
&& (defined(__APPLE__) || defined(FREEBSD) || defined(NETBSD))	/* BSD, Apple */
 #include	<sys/types.h>
 #include	<sys/sysctl.h>
#elif defined(USE_THREADS) && defined(HAVE_MPCTL)		/* HP/UX */
 #include	<sys/mpctl.h>
#endif

#ifdef HAVE_MKL
 #include MKL_H
#endif

#include	"define.h"
#include	"types.h"
#include	"globals.h"
#include	"fits/fitscat.h"
#include	"prefs.h"
#include	"preflist.h"


/********************************* dumpprefs ********************************/
/*
Print the default preference parameters.
*/
void	dumpprefs(int state)
  {
   char	**dp;

  dp = default_prefs;
  while (**dp)
    if (**dp != '*')
      printf("%s\n",*(dp++));
    else if (state)
      printf("%s\n",*(dp++)+1);
    else
      dp++;
  return;
  }


/********************************* readprefs ********************************/
/*
Read a configuration file in ``standard'' format (see the SExtractor
documentation)
*/
void    readprefs(char *filename, char **argkey, char **argval, int narg)

  {
   FILE		*infile;
   char		str[MAXCHARL], errstr[MAXCHAR],
		*cp,  *keyword, *value, **dp;
   int		i, ival, nkey, warn, argi, flagc, flagd, flage, flagz;
   double	dval;
#ifdef	HAVE_GETENV
   static char	value2[MAXCHARL],envname[MAXCHAR];
   char		*dolpos, *listbuf;
#endif


  if ((infile = fopen(filename,"r")) == NULL)
    {
    flage = 1;
    warning(filename, " not found, using internal defaults");
    }
  else
    flage = 0;

/*Build the keyword-list from pkeystruct-array */

  for (i=0; key[i].name[0]; i++)
    strcpy(keylist[i], key[i].name);
  keylist[i][0] = '\0';


/*Scan the configuration file*/

  argi=0;
  flagc = 0;
  flagd = 1;
  dp = default_prefs;
  for (warn=0;;)
    {
    if (flagd)
      {
      if (**dp)
        {
        if (**dp=='*')
          strcpy(str, *(dp++)+1);
        else
          strcpy(str, *(dp++));
        }
      else
        flagd = 0;
      }
    if (!flagc && !flagd)
      if (flage || !fgets(str, MAXCHARL, infile))
        flagc=1;

    if (flagc)
      {
      if (argi<narg)
        {
        sprintf(str, "%s %s", argkey[argi], argval[argi]);
        argi++;
        }
      else
        break;
      }

    keyword = strtok(str, notokstr);
    if (keyword && keyword[0]!=0 && keyword[0]!=(char)'#')
      {
     if (warn>=10)
        error(EXIT_FAILURE, "*Error*: No valid keyword found in ", filename);
      nkey = findkeys(keyword, keylist, FIND_STRICT);
      if (nkey!=RETURN_ERROR)
        {
        value = strtok((char *)NULL, notokstr);
#ifdef	HAVE_GETENV
/*------ Expansion of environment variables (preceded by '$') */
        if (value && (dolpos=strchr(value, '$')))
          {
           int	nc;
           char	*valuet,*value2t, *envval;

          value2t = value2;
          valuet = value;
          while (dolpos)
            {
            while (valuet<dolpos)
              *(value2t++) = *(valuet++);	/* verbatim copy before '$' */
            if (*(++valuet) == (char)'{')
              valuet++;
            strncpy(envname, valuet, nc=strcspn(valuet,"}/:\"\'\\"));
            *(envname+nc) = (char)'\0';
            if (*(valuet+=nc) == (char)'}')
              valuet++;
            if (!(envval=getenv(envname)))
              error(EXIT_FAILURE, "Environment variable not found: ",
				envname);
            while(*envval)			/* Copy the ENV content */
              *(value2t++) = *(envval++);
            while(*valuet && *valuet!=(char)'$')/* Continue verbatim copy */
              *(value2t++) = *(valuet++);
            if (*valuet)
              dolpos = valuet;
            else
              {
              dolpos = NULL;
              *value2t = (char)'\0';
              }
	    }

          value = strtok(value2, notokstr);
          }
#endif
        switch(key[nkey].type)
          {
          case P_FLOAT:
            if (!value || value[0]==(char)'#')
              error(EXIT_FAILURE, keyword," keyword has no value!");
            if (*value=='@')
              value = listbuf = list_to_str(value+1);
            dval = atof(value);
            if (dval>=key[nkey].dmin && dval<=key[nkey].dmax)
              *(double *)(key[nkey].ptr) = dval;
            else
              error(EXIT_FAILURE, keyword," keyword out of range");
            break;

          case P_INT:
            if (!value || value[0]==(char)'#')
              error(EXIT_FAILURE, keyword," keyword has no value!");
            if (*value=='@')
              value = listbuf = list_to_str(value+1);
            ival = (int)strtol(value, (char **)NULL, 0);
            if (ival>=key[nkey].imin && ival<=key[nkey].imax)
              *(int *)(key[nkey].ptr) = ival;
            else
              error(EXIT_FAILURE, keyword, " keyword out of range");
            break;

          case P_STRING:
            if (!value || value[0]==(char)'#')
              value = "";
            if (*value=='@')
              value = listbuf = list_to_str(value+1);
            strcpy((char *)key[nkey].ptr, value);
            break;

          case P_BOOL:
            if (!value || value[0]==(char)'#')
              error(EXIT_FAILURE, keyword," keyword has no value!");
            if (*value=='@')
              value = listbuf = list_to_str(value+1);
            if ((cp = strchr("yYnN", (int)value[0])))
              *(int *)(key[nkey].ptr) = (tolower((int)*cp)=='y')?1:0;
            else
              error(EXIT_FAILURE, keyword, " value must be Y or N");
            break;

          case P_KEY:
            if (!value || value[0]==(char)'#')
              error(EXIT_FAILURE, keyword," keyword has no value!");
            if (*value=='@')
              value = listbuf = list_to_str(value+1);
            if ((ival = findkeys(value, key[nkey].keylist,FIND_STRICT))
			!= RETURN_ERROR)
              *(int *)(key[nkey].ptr) = ival;
            else
              {
              sprintf(errstr, "*Error*: %s set to an unknown keyword: ",
			keyword);
              error(EXIT_FAILURE, errstr, value);
              }
            break;

          case P_BOOLLIST:
            if (value && *value=='@')
              value = strtok(listbuf = list_to_str(value+1), notokstr);
            for (i=0; i<MAXLIST&&value&&value[0]!=(char)'#'; i++)
              {
              if (i>=key[nkey].nlistmax)
                error(EXIT_FAILURE, keyword, " has too many members");
              if ((cp = strchr("yYnN", (int)value[0])))
                ((int *)(key[nkey].ptr))[i] = (tolower((int)*cp)=='y')?1:0;
              else
                error(EXIT_FAILURE, keyword, " value must be Y or N");
              value = strtok((char *)NULL, notokstr);
              }
            if (i<key[nkey].nlistmin)
              error(EXIT_FAILURE, keyword, " list has not enough members");
            *(key[nkey].nlistptr) = i;
            break;

          case P_INTLIST:
            if (value && *value=='@')
              value = strtok(listbuf = list_to_str(value+1), notokstr);
            for (i=0; i<MAXLIST&&value&&value[0]!=(char)'#'; i++)
              {
              if (i>=key[nkey].nlistmax)
                error(EXIT_FAILURE, keyword, " has too many members");
              ival = strtol(value, (char **)NULL, 0);
              if (ival>=key[nkey].imin && ival<=key[nkey].imax)
                ((int *)key[nkey].ptr)[i] = ival;
              else
                error(EXIT_FAILURE, keyword, " keyword out of range");
              value = strtok((char *)NULL, notokstr);
              }
            if (i<key[nkey].nlistmin)
              error(EXIT_FAILURE, keyword, " list has not enough members");
            *(key[nkey].nlistptr) = i;
            break;

          case P_FLOATLIST:
            if (value && *value=='@')
              value = strtok(listbuf = list_to_str(value+1), notokstr);
            for (i=0; i<MAXLIST&&value&&value[0]!=(char)'#'; i++)
              {
              if (i>=key[nkey].nlistmax)
                error(EXIT_FAILURE, keyword, " has too many members");
              dval = atof(value);
              if (dval>=key[nkey].dmin && dval<=key[nkey].dmax)
                ((double *)key[nkey].ptr)[i] = dval;
              else
                error(EXIT_FAILURE, keyword, " keyword out of range");
              value = strtok((char *)NULL, notokstr);
              }
            if (i<key[nkey].nlistmin)
              error(EXIT_FAILURE, keyword, " list has not enough members");
            *(key[nkey].nlistptr) = i;
            break;

          case P_KEYLIST:
            if (value && *value=='@')
              value = strtok(listbuf = list_to_str(value+1), notokstr);
            for (i=0; i<MAXLIST && value && value[0]!=(char)'#'; i++)
              {
              if (i>=key[nkey].nlistmax)
                error(EXIT_FAILURE, keyword, " has too many members");
	      if ((ival = findkeys(value, key[nkey].keylist, FIND_STRICT))
			!= RETURN_ERROR)
                ((int *)(key[nkey].ptr))[i] = ival;
              else
                 {
                sprintf(errstr, "*Error*: %s set to an unknown keyword: ",
			keyword);
                error(EXIT_FAILURE, errstr, value);
                }
              value = strtok((char *)NULL, notokstr);
              }
            if (i<key[nkey].nlistmin)
              error(EXIT_FAILURE, keyword, " list has not enough members");
            *(key[nkey].nlistptr) = i;
            break;

          case P_STRINGLIST:
            if (value && *value=='@')
              value = strtok(listbuf = list_to_str(value+1), notokstr);
            if (!value || value[0]==(char)'#')
              {
              value = "";
              flagz = 1;
              }
            else
              flagz = 0;
            for (i=0; i<MAXLIST && value && value[0]!=(char)'#'; i++)
              {
              if (i>=key[nkey].nlistmax)
                error(EXIT_FAILURE, keyword, " has too many members");
              free(((char **)key[nkey].ptr)[i]);
              QMALLOC(((char **)key[nkey].ptr)[i], char, MAXCHAR);
              strcpy(((char **)key[nkey].ptr)[i], value);
              value = strtok((char *)NULL, notokstr);
              }
            if (i<key[nkey].nlistmin)
              error(EXIT_FAILURE, keyword, " list has not enough members");
            *(key[nkey].nlistptr) = flagz?0:i;
            break;

          default:
            error(EXIT_FAILURE, "*Internal ERROR*: Type Unknown",
				" in readprefs()");
            break;
          }
        key[nkey].flag = 1;
        }
      else
        {
        warning(keyword, " keyword unknown");
        warn++;
        }
      }
    }

  for (i=0; key[i].name[0]; i++)
    if (!key[i].flag)
      error(EXIT_FAILURE, key[i].name, " configuration keyword missing");
  if (!flage)
    fclose(infile);

  return;
  }


/********************************** findkeys **********************************/
/*
 find an item within a list of keywords, SExtractor version.
*/
int	findkeys(char *str, char keyw[][32], int mode)

  {
  int i;

  for (i=0; keyw[i][0]; i++)
    if (!cistrcmp(str, keyw[i], mode))
      return i;

  return RETURN_ERROR;
  }


/******************************* cistrcmp ***********************************/
/*
case-insensitive strcmp.
*/
int     cistrcmp(char *cs, char *ct, int mode)

  {
   int  i, diff;

  if (mode)
    {
    for (i=0; cs[i]&&ct[i]; i++)
      if ((diff=tolower((int)cs[i])-tolower((int)ct[i])))
        return diff;
    }
  else
    {
    for (i=0; cs[i]||ct[i]; i++)
      if ((diff=tolower((int)cs[i])-tolower((int)ct[i])))
        return diff;
    }

  return 0;
  }


/****** list_to_str **********************************************************
PROTO	char    *list_to_str(char *listname)
PURPOSE	Read the content of a file and convert it to a long string.
INPUT	File name.
OUTPUT	Pointer to an allocated string, or NULL if something went wrong.
NOTES	-.
AUTHOR	E. Bertin (IAP)
VERSION	06/02/2008
 ***/
char	*list_to_str(char *listname)
  {
   FILE		*fp;
   char		liststr[MAXCHAR],
		*listbuf, *str;
   int		l, bufpos, bufsize;

  if (!(fp=fopen(listname,"r")))
    error(EXIT_FAILURE, "*Error*: File not found: ", listname);
  bufsize = 8*MAXCHAR;
  QMALLOC(listbuf, char, bufsize);
  for (bufpos=0; fgets(liststr,MAXCHAR,fp);)
    for (str=NULL; (str=strtok(str? NULL: liststr, "\n\r\t "));)
      {
      if (bufpos>MAXLISTSIZE)
        error(EXIT_FAILURE, "*Error*: Too many parameters in ", listname);
      l = strlen(str)+1;
      if (bufpos+l > bufsize)
        {
        bufsize += 8*MAXCHAR;
        QREALLOC(listbuf, char, bufsize);
        }
      if (bufpos)
        listbuf[bufpos-1] = ' ';
      strcpy(listbuf+bufpos, str);
      bufpos += l;
      }
  fclose(fp);

  return listbuf;
  }


/********************************* useprefs **********************************/
/*
Update various structures according to the prefs.
*/
void	useprefs()

  {
   char			str[80],
			*pstr;
   unsigned short	ashort=1;
   int			i, flag;
#ifdef USE_THREADS
   int			nproc;
#endif

/* Test if byteswapping will be needed */
  bswapflag = *((char *)&ashort);

/* Multithreading */
#ifdef USE_THREADS
  if (prefs.nthreads <= 0)
    {
/*-- Get the number of processors for parallel builds */
/*-- See, e.g. http://ndevilla.free.fr/threads */
    nproc = -1;
#if defined(_SC_NPROCESSORS_ONLN)		/* AIX, Solaris, Linux */
    nproc = (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROCESSORS_CONF)
    nproc = (int)sysconf(_SC_NPROCESSORS_CONF);
#elif defined(__APPLE__) || defined(FREEBSD) || defined(NETBSD)	/* BSD, Apple */
    {
     int	mib[2];
     size_t	len;

     mib[0] = CTL_HW;
     mib[1] = HW_NCPU;
     len = sizeof(nproc);
     sysctl(mib, 2, &nproc, &len, NULL, 0);
     }
#elif defined (_SC_NPROC_ONLN)			/* SGI IRIX */
    nproc = sysconf(_SC_NPROC_ONLN);
#elif defined(HAVE_MPCTL)			/* HP/UX */
    nproc =  mpctl(MPC_GETNUMSPUS_SYS, 0, 0);
#endif

    if (nproc>0)
      prefs.nthreads = ((prefs.nthreads) && nproc>(-prefs.nthreads))?
		-prefs.nthreads : nproc;
    else
      {
      prefs.nthreads = prefs.nthreads? -prefs.nthreads : 2;
      sprintf(str, "NTHREADS defaulted to %d", prefs.nthreads);
      warning("Cannot find the number of CPUs on this system:", str);
      }
    }
#ifndef HAVE_ATLAS_MP
   if (prefs.nthreads>1)
     warning("This executable has been compiled using a version of the ATLAS "
	"library without support for multithreading. ",
	"Performance will be degraded.");
#endif
#ifndef HAVE_FFTWF_MP
   if (prefs.nthreads>1)
     warning("This executable has been compiled using a version of the FFTW "
	"library without support for multithreading. ",
	"Performance will be degraded.");
#endif

#ifdef HAVE_MKL
  mkl_set_num_threads(prefs.nthreads);
#endif

#else
  if (prefs.nthreads != 1)
    {
    prefs.nthreads = 1;
    warning("NTHREADS != 1 ignored: ",
	"this build of " BANNER " is single-threaded");
    }
#endif

/* Override INTEL CPU detection routine to help performance on 3rd-party CPUs */
#if defined(__INTEL_COMPILER) && defined (USE_CPUREDISPATCH)
  __get_cpuid(1, &eax, &ebx, &ecx, &edx);
  if (ecx&bit_AVX)
    __intel_cpu_indicator = 0x20000;
  else if (ecx&bit_SSE4_2)
    __intel_cpu_indicator = 0x8000;
  else if (ecx&bit_SSE4_1)
    __intel_cpu_indicator = 0x2000;
  else if (ecx&bit_SSSE3)
    __intel_cpu_indicator = 0x1000;
  else if (ecx&bit_SSE3)
    __intel_cpu_indicator = 0x0800;
  else if (edx&bit_SSE2)
    __intel_cpu_indicator = 0x0200;
  else if (edx&bit_SSE)
    __intel_cpu_indicator = 0x0080;
  else if (edx&bit_MMX)
    __intel_cpu_indicator = 0x0008;
  else
    __intel_cpu_indicator = 0x0001;
#endif

  if (prefs.npsf_size<2)
    prefs.psf_size[1] = prefs.psf_size[0];

  if (prefs.npsf_pixsize<2)
    prefs.psf_pixsize[1] = prefs.psf_pixsize[0];

  if (!prefs.autoselect_flag && !prefs.psf_step)
    warning("SAMPLE_AUTOSELECT set to N and PSF_SAMPLING set to 0.0:\n",
		" PSF_SAMPLING will default to 1 pixel");

/*---------------------------- Measurement arrays --------------------------*/
  strcpy(prefs.photflux_rkey, prefs.photflux_key);
  strtok(prefs.photflux_rkey,"([{}])");
  prefs.photflux_num = (pstr = strtok(NULL,"([{}])"))? atoi(pstr) : 1;
  strcpy(prefs.photfluxerr_rkey, prefs.photfluxerr_key);
  strtok(prefs.photfluxerr_rkey,"([{}])");
  prefs.photfluxerr_num = (pstr = strtok(NULL,"([{}])"))? atoi(pstr) : 1;

/*------------------------------- Contexts ---------------------------------*/
  if ((prefs.group_deg[0]!=0) && prefs.ncontext_group != prefs.ncontext_name)
    error(EXIT_FAILURE, "*Error*: PSFVAR_GROUPS and PSFVAR_KEYS do not ",
			"match");
  if (!prefs.group_deg[0])
    prefs.ncontext_group = 0;
  for (i=0; i<prefs.ncontext_group; i++)
    if (prefs.context_group[i]>prefs.ngroup_deg)
      error(EXIT_FAILURE, "*Error*: PSFVAR_GROUPS out of range for ",
			prefs.context_name[i]);

/* ngroup_deg implicitely states the number of groups (one degree per group) */
  if (!prefs.ncontext_group)
    prefs.ngroup_deg = 0;

/*---------------------------- Common/independent MEF ----------------------*/
  if (prefs.newbasis_type == NEWBASIS_PCAINDEPENDENT
	&& prefs.psf_mef_type == PSF_MEF_COMMON)
    {
    prefs.newbasis_type = NEWBASIS_PCACOMMON;
    warning("NEWBASIS_TYPE PCA_INDEPENDENT is incompatible with "
	"MEF_TYPE COMMON:\n", " NEWBASIS_TYPE forced to PCA_COMMON");
    }

  flag = 0;
  for (i=0; i<prefs.ncontext_name; i++)
    if (!wstrncmp(prefs.context_name[i], "HIDDEN?", 80))
      {
      flag = 1;
      break;
      }
  if (flag && prefs.hidden_mef_type == HIDDEN_MEF_INDEPENDENT
	&& prefs.psf_mef_type == PSF_MEF_COMMON)
    {
    prefs.hidden_mef_type = HIDDEN_MEF_COMMON;
    warning("HIDDENMEF_TYPE INDEPENDENT is incompatible with "
	"MEF_TYPE COMMON:\n", " HIDDENMEF_TYPE forced to COMMON");
    }

/*--------------------------- Output directories ----------------------------*/
/* Strip final "/" if present */
  if ((i=strlen(prefs.psf_dir)-1) > 0 && *(pstr=prefs.psf_dir+i) == (char)'/')
    *pstr = (char)'\0';
  if ((i=strlen(prefs.homokernel_dir)-1) > 0
	&& *(pstr=prefs.homokernel_dir+i) == (char)'/')
    *pstr = (char)'\0';

/*----------------------------- CHECK-images -------------------------------*/
  flag = 0;
  for (i=0; i<prefs.ncheck_type; i++)
    if (prefs.check_type[i] != PSF_NONE)	/* at least 1 is not NONE */
      flag = 1;

  if (flag && prefs.ncheck_name!=prefs.ncheck_type)
    error(EXIT_FAILURE, "*Error*: CHECKIMAGE_NAME(s) and CHECKIMAGE_TYPE(s)",
		" are not in equal number");

  return;
  }



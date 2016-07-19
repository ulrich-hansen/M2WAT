/*
 M2WAT - Convert mTCP configuration file to WATTCP format.

Copyright (C) 2011 by Cordata

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


 This program uses the environment variable MTCPCFG to find
 an mTCP configuration file.  This file is interesting to the
 WATTCP user because it may contain IP address information from
 DHCP.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <conio.h>
#include <ctype.h>

#ifdef __WATCOMC__
#define mktemp _mktemp
#endif

#define MAX_CFG 4     /* increase this if more data from mTCP goes to WATTCP */
#define MAX_ALIAS 2   /* increase if a WATTCP parameter has more names */

#define NO_ALIAS  ""
#define LINE_LEN  81

typedef struct cfg_dat
{
char *mtcp_name;                /* how does mTCP refer to parameter */
char *wattcp_name[MAX_ALIAS];   /* how does WATTCP refer to parameter */
char mtcp_value[LINE_LEN];      /* what is the mTCP value for parameter */
int  written_2_wat;             /* has parameter been written to WATTCP file */
}  cfg_dat_type;

/* the following table can be used to add items from mTCP to push
   into the WATTCP file.   To add new items just add entries to this
   table.  We account for WATTCP having multiple names for parameters.
   In particular the IP address can be called "IP" or "MY_IP".

   */
cfg_dat_type  cfg_data[MAX_CFG] =
{
{ "IPADDR","MY_IP","IP","",0},
{ "GATEWAY","GATEWAY",NO_ALIAS,"",0},
{ "NETMASK","NETMASK",NO_ALIAS,"",0},
{ "NAMESERVER","NAMESERVER",NO_ALIAS,"",0},
};

/* function prototypes */
void adjust_wat_line (char *wat_line, char *dst);
void store_mtcp_line(char *);
void strip(char *);
void usage(void);

void main(int argc, char *argv[])
{
char *mtcp, *wattcp;  /* environment variable pointers */

char str[LINE_LEN],w_fname[LINE_LEN],tmp_fname[LINE_LEN];

FILE *m_file, *w_file, *new_w_file;

int l,p,new_file=0;

if (argc != 1)
  usage();

if (!(mtcp=getenv("MTCPCFG")))
   {
   fprintf(stderr,"ERROR: must set MTCPCFG environment variable\n");
   exit(1);
   }

m_file = fopen(mtcp,"r");

if (!m_file) 
   {
   fprintf(stderr,"Error opening mTCP config file %s\n",mtcp);
   exit(1);
   }

while (fgets(str,LINE_LEN -1,m_file))
   {
   str[LINE_LEN -1]='\0';
   strip(str);
   store_mtcp_line(str);
   }
fclose(m_file);


if (!(wattcp=getenv("WATTCP.CFG")))
    {
    printf("No WATTCP.CFG Environment variable. Using current directory.\n");
    wattcp=".";
    }
   
strcpy(w_fname,wattcp);
w_fname[LINE_LEN -1 ]='\0';
l=strlen(w_fname);
if (l > (LINE_LEN - 12))
 {
 fprintf(stderr,"ERROR: WATTCP directory too long.\n");
 exit(1);
 }

if ((w_fname[l-1] != '\\') && (w_fname[l-1] != '/'))
  strcat(w_fname,"\\");

strcpy(tmp_fname, w_fname); /* same directory for temp file */
strcat(tmp_fname,"XXXXXX");
strcat(w_fname,"WATTCP.CFG");

if (!(w_file=fopen(w_fname,"r")))
  {
  char ch;
  printf("WATTCP config file %s does not exist.\n",w_fname);
  printf("Do you want to create it? (Y/N)");
  do ch=toupper(getch()); 
     while ((ch != 'Y') && (ch != 'N'));
  if (ch == 'N')
    exit(0);
  new_file=1;
  }
  if (!mktemp(tmp_fname))
     {
     printf("Error: Unable to create temporary file.\n");
     exit(1);
     }

   new_w_file= fopen(tmp_fname,"w");

if (!new_file)
{
while (fgets(str,LINE_LEN -1,w_file))  /* read lines from WATTCP file */
   {
   char buf[LINE_LEN];
   str[LINE_LEN -1]='\0';
   strip(str);
   adjust_wat_line(str,buf);   /* add mTCP info to line, comment out or do nothing*/
   fprintf(new_w_file,"%s\n",buf); /* write to new file*/
   }
fclose(w_file);
}

/* now check to see if we missed anything */
  for (p=0;p<MAX_CFG;p++)
    if (( cfg_data[p].written_2_wat==0)  && strlen(cfg_data[p].mtcp_value))
      fprintf(new_w_file,"%s = %s\n",cfg_data[p].wattcp_name[0],cfg_data[p].mtcp_value);
 fclose(new_w_file);
 unlink(w_fname);
 rename(tmp_fname, w_fname);
 printf("\nWATTCP.CFG updated successfully.\n");
 exit(0);
}

void store_mtcp_line (char *mtcp_line)
{
/* this function will parse the input line from the MTCP file.  First it
   checks if the line contains something "interesting" and if so it stores
   the value in our data structure */
int parm;

char *parm_ptr,tmp[LINE_LEN];

strcpy(tmp, mtcp_line);
if (!(parm_ptr = strtok(tmp," \t"))) /* today MTCP does not use tab but may in future*/
     return;  /* if can't parse skip */
for (parm=0;parm<MAX_CFG;parm++)
   if (!stricmp(cfg_data[parm].mtcp_name,parm_ptr)) /* found it */
      {
      char *s;
      s=&mtcp_line[strlen(parm_ptr)+1]; /* point to 1st char after parm name */
      strcpy(cfg_data[parm].mtcp_value, s); /* value is all but parm name */
//      printf("Found parameter %s  value: %s\n",parm_ptr,s);
      return;
      }
}


void adjust_wat_line (char *wat_line, char *dst)
{
/* This function will adjust the WATTCP config file line to 
   conform with what we found in the MTCP config file.
   Idea is that if the line does not start with one of the WATTCP
   parameter names or aliases the line is unchanged.
   If the line does start with one of the key aliases AND that value
   has already been written the line will be prepended with a comment ('#')
   Otherwise the MTCP values will be appended to the WATTCP keyword.

*/
int parm, alias;
char *w_parm, tmp[LINE_LEN];
strcpy(tmp,wat_line);  /* store in buffer since strtok changes */
/* first get the WATTCP parameter name */
w_parm = strtok(tmp," \t="); /* first thing followed by space, tab or equals */

if (w_parm)
{
for (parm=0;parm < MAX_CFG;parm++)
  for(alias=0;alias<MAX_ALIAS; alias++)
    if (!stricmp(w_parm,cfg_data[parm].wattcp_name[alias])) /* found it */
       {
       if (cfg_data[parm].written_2_wat) /* already used */
           {
           sprintf(dst,"#%s",wat_line);
           return;
           }
       else /* not used yet */
          {
          cfg_data[parm].written_2_wat=1; /* mark used */
          sprintf(dst,"%s = %s",cfg_data[parm].wattcp_name[alias],cfg_data[parm].mtcp_value);
          return;
          }
       }
  }  /* if found a parameter */
  strcpy(dst,wat_line);
}


void strip(char *s)
/* removes carriage return and line feed characters from a string. 
*/
{
char *dst,*src;
src=s;
dst=s;

while(*src)
  {
  *dst = *src;
  if ( (*dst == '\r') || (*dst == '\n'))
      {
      src++;
      continue;      
      }  
  src++;
  dst++;
  }
 *dst='\0';
}


void usage (void)
{
printf("M2WAT - Convert mTCP configuration file to WATTCP file\n");
printf("Copyright 2011 by Cordata\n\n");
printf("M2WAT will edit the WATTCP configuration file and input the\n");
printf("configuration parameters which are contained in the mTCP config\n");
printf("file.  This would normally be done when using the mTCP DHCP utility.\n");
printf("The MTCP.CFG environment variable is used to find the MTCP config file.\n");
printf("The WATTCP.CFG environment variable is optional, if not present the\n");
printf("current working directory is used.  Note that the MTCP.CFG variable\n");
printf("must refer to a file while the WATTCP.CFG variable refers to a directory.\n");
printf("The WATTCP.CFG file is always called WATTCP.CFG.  If the WATTCP.CFG\n");
printf("file does not exist the option is provided to create it.\n");
printf("Currently M2WAT will add the IP address, Gateway, Netmask and nameserver\n");
printf("to the WATTCP.CFG file from the mTCP config file.\n");
printf("\nTo use M2WAT there are no command line parameters or options.\n");
printf("All information is taken from the environment variables.\n");
exit(1);
}

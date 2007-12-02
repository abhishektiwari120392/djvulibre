//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------
// 
// $Id$


/* Program ddjvu has been rewritten to use the ddjvuapi only.
 * This file should compile both as C and C++. 
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <fcntl.h>
#include <errno.h>

#ifdef UNIX
# include <sys/time.h>
#endif

#include "libdjvu/ddjvuapi.h"

#if defined(WIN32) || defined(__CYGWIN32__)
# include <io.h>
#endif
#if HAVE_PUTC_UNLOCKED
# undef putc
# define putc putc_unlocked
#endif
#if HAVE_TIFF
# include <tiffio.h>
# ifndef COMPRESSION_CCITT_T6
#  define COMPRESSION_CCITT_T6 4
# endif
# ifndef COMPRESSION_JPEG
#  define COMPRESSION_JPEG 7
# endif
# ifndef COMPRESSION_PACKBITS
#  define COMPRESSION_PACKBITS 32771
# endif
#endif

/* Some day we'll redo i18n right. */
#ifndef i18n
# define i18n(x) (x)
# define I18N(x) (x)
#endif


unsigned long 
ticks(void)
{
#ifdef UNIX
  struct timeval tv;
  if (gettimeofday(&tv, NULL) >= 0)
    return (unsigned long)(((tv.tv_sec & 0xfffff)*1000)+(tv.tv_usec/1000));
#endif
  return 0;
}

ddjvu_context_t *ctx;
ddjvu_document_t *doc;

unsigned long timingdata[4];

double       flag_scale = -1;
int          flag_size = -1;
int          flag_aspect = -1;
int          flag_subsample = -1;
int          flag_segment = 0;
int          flag_verbose = 0;
char         flag_mode = 0;     /* 'c', 'k', 's', 'f','b' */
char         flag_format = 0;   /* '4','5','6','p','r','t' */
int          flag_quality = -1;
const char  *flag_pagespec = 0; 
ddjvu_rect_t info_size;
ddjvu_rect_t info_segment;
const char  *inputfilename;
const char  *outputfilename;

FILE *fout;
#if HAVE_TIFF
TIFF *tiff;
#endif



/* Djvuapi events */

void
handle(int wait)
{
  const ddjvu_message_t *msg;
  if (!ctx)
    return;
  if (wait)
    msg = ddjvu_message_wait(ctx);
  while ((msg = ddjvu_message_peek(ctx)))
    {
      switch(msg->m_any.tag)
        {
        case DDJVU_ERROR:
          fprintf(stderr,"ddjvu: %s\n", msg->m_error.message);
          if (msg->m_error.filename)
            fprintf(stderr,"ddjvu: '%s:%d'\n", 
                    msg->m_error.filename, msg->m_error.lineno);
          exit(10);
        default:
          break;
        }
      ddjvu_message_pop(ctx);
    }
}


void 
die(const char *fmt, ...)
{
  /* Handling messages might give a better error message */
  handle(FALSE);
  /* Print */
  va_list args;
  fprintf(stderr,"ddjvu: ");
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr,"\n");
  /* Terminates */
  exit(10);
}


void
inform(ddjvu_page_t *page, int pageno)
{
  if (flag_verbose)
    {
      const char *desctype;
      char *description = ddjvu_page_get_long_description(page);
      ddjvu_page_type_t type = ddjvu_page_get_type(page);
      fprintf(stderr,i18n("\n-------- page %d -------\n"), pageno);
      if (type == DDJVU_PAGETYPE_BITONAL)
        desctype = i18n("This is a legal Bitonal DjVu image");
      else if (type == DDJVU_PAGETYPE_PHOTO)
        desctype = i18n("This is a legal Photo DjVu image");
      else if (type == DDJVU_PAGETYPE_COMPOUND)
        desctype = i18n("This is a legal Compound DjVu image");
      else
        desctype = i18n("This is a malformed DjVu image");
      fprintf(stderr,"%s.\n", desctype);
      if (description)
        fprintf(stderr,"%s\n", description);
      if (description)
        free(description);
      if (timingdata[0] != timingdata[1])
	fprintf(stderr,"Decoding time:  %5ld ms\n",
		timingdata[1] - timingdata[0] );
    }
}


void
render(ddjvu_page_t *page)
{
  ddjvu_rect_t prect;
  ddjvu_rect_t rrect;
  ddjvu_format_style_t style;
  ddjvu_render_mode_t mode;
  ddjvu_format_t *fmt;
  int iw = ddjvu_page_get_width(page);
  int ih = ddjvu_page_get_height(page);
  int dpi = ddjvu_page_get_resolution(page);
  ddjvu_page_type_t type = ddjvu_page_get_type(page);
  char *image = 0;
  int rowsize;
#if HAVE_TIFF
  int compression = COMPRESSION_NONE;
#endif
  
  /* Process size specification */
  prect.x = 0;
  prect.y = 0;
  if (flag_size > 0)
    {
      prect.w = info_size.w;
      prect.h = info_size.h;
    }
  else if (flag_subsample > 0)
    {
      prect.w = (iw + flag_subsample - 1) / flag_subsample;
      prect.h = (ih + flag_subsample - 1) / flag_subsample;
    }
  else if (flag_scale > 0)
    {
      prect.w = (unsigned int) (iw * flag_scale) / dpi;
      prect.h = (unsigned int) (ih * flag_scale) / dpi;
    }
  else if (flag_format)
    {
      prect.w = iw;
      prect.h = ih;
    }
  else
    {
      prect.w = (iw * 100) / dpi;
      prect.h = (ih * 100) / dpi;
    }
  /* Process aspect ratio */
  if (flag_aspect <= 0)
    {
      double dw = (double)iw / prect.w;
      double dh = (double)ih / prect.h;
      if (dw > dh) 
        prect.h = (int)(ih / dw);
      else
        prect.w = (int)(iw / dh);
    }

  /* Process segment specification */
  rrect = prect;
  if (flag_segment > 0)
    {
      rrect = info_segment;
      if (rrect.x < 0)
        rrect.x = prect.w - rrect.w + rrect.x;
      if (rrect.y < 0)
        rrect.y = prect.h - rrect.h + rrect.y;
    }

  /* Process mode specification */
  mode = DDJVU_RENDER_COLOR;
  if (flag_mode == 'f')
    mode = DDJVU_RENDER_FOREGROUND;
  else if (flag_mode == 'b')
    mode = DDJVU_RENDER_BACKGROUND;
  else if (flag_mode == 'c')
    mode = DDJVU_RENDER_COLOR;
  else if (flag_mode == 'k')
    mode = DDJVU_RENDER_BLACK;
  else if (flag_mode == 's')
    mode = DDJVU_RENDER_MASKONLY;
  else if (flag_format == 'r' || flag_format == '4')
    mode = DDJVU_RENDER_BLACK;

  /* Determine output pixel format */
  style = DDJVU_FORMAT_RGB24;
  if ((mode==DDJVU_RENDER_BLACK) ||
      (mode==DDJVU_RENDER_MASKONLY) ||
      (mode==DDJVU_RENDER_COLOR) && (type==DDJVU_PAGETYPE_BITONAL))
    {
      style = DDJVU_FORMAT_GREY8;
      if ((int)prect.w == iw && (int)prect.h == ih)
        style = DDJVU_FORMAT_MSBTOLSB;
    }
  switch(flag_format)
    {
    case 't':
#if HAVE_TIFF
      compression = COMPRESSION_NONE;
      if (style==DDJVU_FORMAT_MSBTOLSB 
          && TIFFFindCODEC(COMPRESSION_CCITT_T6))
        compression = COMPRESSION_CCITT_T6;
      else if (style!=DDJVU_FORMAT_MSBTOLSB && flag_quality>0 
               && TIFFFindCODEC(COMPRESSION_JPEG))
        compression = COMPRESSION_JPEG;
      else if (TIFFFindCODEC(COMPRESSION_PACKBITS))
        compression = COMPRESSION_PACKBITS;
      break;
#endif      
    case '4':
      style = DDJVU_FORMAT_MSBTOLSB; 
      break;
    case 'r': 
    case '5':
      style = DDJVU_FORMAT_GREY8;    
      break;
    case '6':
      style = DDJVU_FORMAT_RGB24;   
      break;
    default:
      break;
    }
  if (! (fmt = ddjvu_format_create(style, 0, 0)))
    die(i18n("Cannot determine pixel style"));
  ddjvu_format_set_row_order(fmt, 1);
  /* Allocate buffer */
  if (style == DDJVU_FORMAT_MSBTOLSB)
    rowsize = (rrect.w + 7) / 8;
  else if (style == DDJVU_FORMAT_GREY8)
    rowsize = rrect.w;
  else
    rowsize = rrect.w * 3; 
  if (! (image = (char*)malloc(rowsize * rrect.h)))
    die(i18n("Cannot allocate image buffer"));

  /* Render */
  timingdata[2] = ticks();
  if (! ddjvu_page_render(page, mode, &prect, &rrect, fmt, rowsize, image))
    die(i18n("Cannot render image"));
  timingdata[3] = ticks();
  if (flag_verbose)
    if (timingdata[2] != timingdata[3])
      fprintf(stderr,"Rendering time: %5ld ms\n",
	      timingdata[3] - timingdata[2] );

  /* Output */
  switch (flag_format)
    {
      /* -------------- PNM output */
    default:
    case '4':
    case '5':
    case '6':
      {
        int i;
        char *s = image;
        if (style == DDJVU_FORMAT_MSBTOLSB) {
          if (flag_verbose) 
            fprintf(stderr,i18n("Producing PBM file.\n"));
          fprintf(fout,"P4\n%d %d\n", rrect.w, rrect.h);
        } else if (style == DDJVU_FORMAT_GREY8) {
          if (flag_verbose) 
            fprintf(stderr,i18n("Producing PGM file.\n"));
          fprintf(fout,"P5\n%d %d\n255\n", rrect.w, rrect.h);
        } else {
          if (flag_verbose) 
            fprintf(stderr,i18n("Producing PPM file.\n"));
          fprintf(fout,"P6\n%d %d\n255\n", rrect.w, rrect.h);
        }
        for (i=0; i<(int)rrect.h; i++,s+=rowsize)
          if (fwrite(s, 1, rowsize, fout) < (size_t)rowsize)
            die(i18n("writing pnm file: %s"), strerror(errno));
        break;
      }
      /* -------------- RLE output */
    case 'r':
      {
        int i;
        unsigned char *s = (unsigned char *)image;
        if (flag_verbose)
          fprintf(stderr,i18n("Producing RLE file.\n"));
        fprintf(fout,"R4\n%d %d\n", rrect.w, rrect.h);
        for (i=0; i<(int)rrect.h; i++,s+=rowsize)
          {
            int j = 0;
            int c = 0xff;
            while (j < (int)rrect.w)
              {
                int l = j;
                while ((j<(int)rrect.w) && ((s[j]^c)<128))
                  j += 1;
                c = c ^ 0xff;
                l = j - l;
                while (l > 0x3fff) {
                  putc( 0xff, fout);
                  putc( 0xff, fout);
                  putc( 0x00, fout);
                  l -= 0x3fff;
                }
                if (l > 0xbf) {
                  putc( (l >> 8) + 0xc0, fout);
                  putc( (l & 0xff), fout);
                } else {
                  putc( l, fout);
                }
              }
          }
        if (ferror(fout))
          die(i18n("writing rle file: %s"), strerror(errno));
        break;
      }
      /* -------------- TIFF output */
    case 't':
      {
#if HAVE_TIFF
        int i;
        char *s = image;
        TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, (uint32)rrect.w);
        TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, (uint32)rrect.h);
        TIFFSetField(tiff, TIFFTAG_XRESOLUTION, 
		     (float)((dpi*rrect.w+iw/2)/iw));
        TIFFSetField(tiff, TIFFTAG_YRESOLUTION, 
		     (float)((dpi*rrect.h+ih/2)/ih));
        TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, (uint32)64);
        if (style == DDJVU_FORMAT_MSBTOLSB) {
          TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, (uint16)1);
          TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, (uint16)1);
          TIFFSetField(tiff, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
          TIFFSetField(tiff, TIFFTAG_COMPRESSION, compression);
          TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
        } else {
          TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, (uint16)8);
          if (style == DDJVU_FORMAT_GREY8) {
            TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, (uint16)1);
            TIFFSetField(tiff, TIFFTAG_COMPRESSION, compression);
            TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
          } else {
            TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, (uint16)3);
            TIFFSetField(tiff, TIFFTAG_COMPRESSION, compression);
            if (compression == COMPRESSION_JPEG) {
              TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_YCBCR);
              TIFFSetField(tiff, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
              TIFFSetField(tiff, TIFFTAG_JPEGQUALITY, flag_quality);
            } else 
              TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
          }
        }
        if (flag_verbose) {
          if (compression == COMPRESSION_CCITT_T6)
            fprintf(stderr,i18n("Producing TIFF/G4 file.\n"));
          else if (compression == COMPRESSION_JPEG)
            fprintf(stderr,i18n("Producing TIFF/JPEG file.\n"));
          else if (compression == COMPRESSION_PACKBITS)
            fprintf(stderr,i18n("Producing TIFF/PACKBITS file.\n"));
          else
            fprintf(stderr,i18n("Producing TIFF file.\n"));
        }
        if (rowsize != TIFFScanlineSize(tiff))
          die("internal error (%d!=%d)", rowsize, (int)TIFFScanlineSize(tiff));
        for (i=0; i<(int)rrect.h; i++,s+=rowsize)
          TIFFWriteScanline(tiff, s, i, 0);
#else
        die(i18n("TIFF output is not compiled"));
#endif
      }
    }

  /* Free */
  ddjvu_format_release(fmt);
  free(image);
}



void
dopage(int pageno)
{
  ddjvu_page_t *page;
  /* Decode page */
  timingdata[0] = ticks();
  if (! (page = ddjvu_page_create_by_pageno(doc, pageno-1)))
    die(i18n("Cannot access page %d."), pageno);
  while (! ddjvu_page_decoding_done(page))
    handle(TRUE);
  if (ddjvu_page_decoding_error(page))
    die(i18n("Cannot decode page %d."), pageno);
  
  timingdata[1] = ticks();
  /* Open files */
  if (flag_format == 't') 
    {
#if HAVE_TIFF
      if (! tiff) 
        {
          if (! strcmp(outputfilename,"-"))
            die(i18n("Tiff output requires a valid output file name."));
          else if (! (tiff = TIFFOpen(outputfilename, "w")))
            die(i18n("Cannot open output tiff file '%s'."), outputfilename);
        } 
      else 
        {
          if (! TIFFWriteDirectory(tiff))
            die(i18n("Problem writing TIFF directory."));
        }
#else
      die(i18n("TIFF output is not compiled"));
#endif
    } 
  else if (! fout) 
    {
      if (! strcmp(outputfilename,"-")) {
        fout = stdout;
#if defined(__CYGWIN32__)
        setmode(fileno(fout), O_BINARY);
#elif defined(WIN32)
        _setmode(_fileno(fout), _O_BINARY);
#endif
      } else if (! (fout = fopen(outputfilename, "wb")))
        die(i18n("Cannot open output file '%s'."), outputfilename);
    }
  /* Render */
  inform(page, pageno);
  render(page);
  ddjvu_page_release(page);
}



void
parse_pagespec(const char *s, int max_page, void (*dopage)(int))
{
  static const char *err = 
    I18N("invalid page specification: %s");
  int spec = 0;
  int both = 1;
  int start_page = 1;
  int end_page = max_page;
  int pageno;
  char *p = (char*)s;
  while (*p)
    {
      spec = 0;
      while (*p==' ')
        p += 1;
      if (! *p)
        break;
      if (*p>='0' && *p<='9') {
        end_page = strtol(p, &p, 10);
        spec = 1;
      } else if (*p=='$') {
        spec = 1;
        end_page = max_page;
        p += 1;
      } else if (both) {
        end_page = 1;
      } else {
        end_page = max_page;
      }
      while (*p==' ')
        p += 1;
      if (both) {
        start_page = end_page;
        if (*p == '-') {
          p += 1;
          both = 0;
          continue;
        }
      }
      both = 1;
      while (*p==' ')
        p += 1;
      if (*p && *p != ',')
        die(i18n(err), s);
      if (*p == ',')
        p += 1;
      if (! spec)
        die(i18n(err), s);
      if (end_page < 0)
        end_page = 0;
      if (start_page < 0)
        start_page = 0;
      if (end_page > max_page)
        end_page = max_page;
      if (start_page > max_page)
        start_page = max_page;
      if (start_page <= end_page)
        for(pageno=start_page; pageno<=end_page; pageno++)
          (*dopage)(pageno);
      else
        for(pageno=start_page; pageno>=end_page; pageno--)
          (*dopage)(pageno);
    }
  if (! spec)
    die(i18n(err), s);
}



void
parse_geometry(const char *s, ddjvu_rect_t *r)
{
  static const char *fmt = 
    I18N("syntax error in geometry specification: %s");
  char *curptr = (char*) s;
  char *endptr;

  r->w = strtol(curptr, &endptr, 10);
  if (endptr<=curptr || r->w<=0 || *endptr!='x')
    die(i18n(fmt), s);
  curptr = endptr+1;
  r->h = strtol(curptr, &endptr, 10);
  if (endptr<=curptr || r->h<=0)
    die(i18n(fmt), s);
  curptr = endptr;
  r->x = 0;
  r->y = 0;
  if (curptr[0])
    {
      if (curptr[0]=='+')
        curptr++;
      else if (curptr[0]!='-')
        die(i18n(fmt), s);
      r->x = strtol(curptr, &endptr, 10);
      curptr = endptr;
      if (curptr[0])
        {
          if (curptr[0]=='+')
            curptr++;
          else if (curptr[0]!='-')
            die(i18n(fmt), s);
          r->y = strtol(curptr, &endptr, 10);
          if (endptr[0])
            die(i18n(fmt), s);
        }
    }
}



void
usage()
{
#ifdef DJVULIBRE_VERSION
  fprintf(stderr, "DDJVU --- DjVuLibre-" DJVULIBRE_VERSION "\n");
#endif
  fprintf(stderr, "%s",
    i18n("DjVu decompression utility\n\n"
         "Usage: ddjvu [options] [<djvufile> [<outputfile>]]\n\n"
         "Options:\n"
         "  -verbose          Prints various informational messages.\n"
         "  -format=FMT       Selects output format: pbm,pgm,ppm,pnm,rle,tiff.\n"
         "  -scale=N          Selects display scale.\n"
         "  -size=WxH         Selects size of rendered image.\n"
         "  -subsample=N      Selects direct subsampling factor.\n"
         "  -aspect=no        Authorizes aspect ratio changes\n"
         "  -segment=WxH+X+Y  Selects which segment of the rendered image\n"
         "  -mode=black       Renders a meaningful bitonal image.\n"
         "  -mode=mask        Only renders the mask layer.\n"
         "  -mode=foreground  Only renders the foreground layer.\n"
         "  -mode=background  Only renders the background layer.\n"
         "  -page=PAGESPEC    Selects page(s) to be decoded.\n"
         "  -quality=QUALITY  Specifies jpeg quality for lossy tiff output.\n"
         "\n"
         "If <outputfile> is a single dash or omitted, the decompressed image\n"
         "is sent to the standard output.  If <djvufile> is a single dash or\n"
         "omitted, the djvu file is read from the standard input.\n\n" ));
  exit(1);
}



int 
parse_option(int argc, char **argv, int i)
{
  static const char *errarg = 
    I18N("option '-%s' needs no argument.");
  static const char *errnoarg = 
    I18N("option '-%s' needs an argument.");
  static const char *errbadarg = 
    I18N("valid arguments for option '-%s' %s.");
  static const char *errdupl = 
    I18N("option '%s' specified multiple times.");
  static const char *errconfl = 
    I18N("option '%s' conflicts with another option.");
  
  char buf[32];
  const char *s = argv[i];
  const char *opt = s;
  const char *arg = strchr(opt, '=');
  char *end;

  /* Split argument */
  if (*opt == '-')
    opt += 1;
  if (*opt == '-')
    opt += 1;
  if (arg) {
    int l = arg - opt;
    if (l > (int)sizeof(buf) - 1)
      l = sizeof(buf) - 1;
    strncpy(buf, opt, l);
    buf[l] = 0;
    opt = buf;
    arg += 1;
  }

  /* Legacy options */
  if (!strcmp(opt, "black") ||
      !strcmp(opt, "foreground") ||              
      !strcmp(opt, "background") ) 
    {
      arg = opt;
      opt = "mode";
    } 
  else if (!strcmp(opt,"rle"))
    {
      arg = opt;
      opt = "format";
    }
  else if (!strcmp(opt,"segment") ||
           !strcmp(opt,"scale") ||
           !strcmp(opt,"size") ||
           !strcmp(opt,"page") ) 
    {
      if (!arg && i<argc-1)
        if (argv[i+1][0]>='0' && argv[i+1][0]<='9')
          arg = argv[++i];
    }
  else if (strtol(opt,&end,10) && !*end)
    {
      arg = opt;
      opt = "subsample";
    }
  
  /* Parse options */
  if (!strcmp(opt,"v") ||
      !strcmp(opt,"verbose"))
    {
      if (arg) 
        die(i18n(errarg), opt);
      flag_verbose = 1;
    }
  else if (!strcmp(opt,"scale"))
    {
      if (!arg) 
        die(i18n(errnoarg), opt);
      if (flag_subsample>=0 || flag_scale>=0 || flag_size>=0)
        die(i18n(errconfl), s);
      flag_scale = strtol(arg, &end, 10);
      if (*end == '%')
        end++;
      if (*end || flag_scale<1 || flag_scale>999)
        die(i18n(errbadarg), opt, i18n("range from 1% to 999%"));
    }
  else if (!strcmp(opt,"aspect"))
    {
      if (flag_aspect >= 0)
        die(i18n(errdupl), opt);
      if (!arg || !strcmp(arg,"no"))
        flag_aspect = 1;
      else if (!strcmp(arg,"yes"))
        flag_aspect = 0;
      else
        die(i18n(errbadarg), opt, i18n("are 'yes' or 'no'"));
    }
  else if (!strcmp(opt,"size"))
    {
      if (!arg) 
        die(i18n(errnoarg), opt);
      if (flag_subsample>=0 || flag_scale>=0 || flag_size>=0)
        die(i18n(errconfl), s);
      parse_geometry(arg, &info_size);
      if (info_size.x || info_size.y)
        die(i18n(errbadarg), opt, i18n("have the form <width>x<height>"));
      flag_size = 1;
    }
  else if (!strcmp(opt,"subsample"))
    {
      if (!arg) 
        die(i18n(errnoarg), opt);
      if (flag_subsample>=0 || flag_scale>=0 || flag_size>=0)
        die(i18n(errconfl), s);
      flag_subsample = strtol(arg, &end, 10);
      if (*end || flag_subsample<1)
        die(i18n(errbadarg),opt,i18n("are positive integers"));
    }
  else if (!strcmp(opt,"segment"))
    {
      if (!arg) 
        die(i18n(errnoarg), opt);
      if (flag_segment)
        die(i18n(errdupl), opt);
      parse_geometry(arg, &info_segment);
      flag_segment = 1;
    }
  else if (!strcmp(opt,"format"))
    {
      if (!arg) 
        die(i18n(errnoarg), opt);
      if (flag_format)
        die(i18n(errdupl), opt);
      if (!strcmp(arg,"pbm"))
        flag_format='4';
      else if (!strcmp(arg,"pgm"))
        flag_format='5';
      else if (!strcmp(arg,"ppm"))
        flag_format='6';
      else if (!strcmp(arg,"pnm"))
        flag_format='p';
      else if (!strcmp(arg,"rle"))
        flag_format='r';
      else if (!strcmp(arg,"tiff") || !strcmp(arg,"tif"))
        flag_format='t';
      else
        die(i18n(errbadarg),opt,i18n("are: pbm,pgm,ppm,pnm,rle,tiff"));
    }
  else if (!strcmp(opt,"mode"))
    {
      if (!arg) 
        die(i18n(errnoarg), opt);
      if (flag_mode)
        die(i18n(errdupl), opt);
      if (!strcmp(arg,"color") )
        flag_mode = 'c';
      else if (!strcmp(arg,"black"))
        flag_mode = 'k';
      else if (!strcmp(arg,"mask") || !strcmp(arg,"stencil"))
        flag_mode = 's';
      else if (!strcmp(arg,"foreground") || !strcmp(arg,"fg"))
        flag_mode = 'f';
      else if (!strcmp(arg,"background") || !strcmp(arg,"bg"))
        flag_mode = 'b';
      else
        die(i18n(errbadarg),opt,i18n("are: color,black,mask,fg,bg"));
    }
  else if (! strcmp(opt, "page") ||
           ! strcmp(opt, "pages") )
    {
      if (!arg) 
        die(i18n(errnoarg), opt);
      if (flag_pagespec)
        die(i18n(errdupl), opt);
      flag_pagespec = arg;
    }
  else if (!strcmp(opt,"quality") ||
           !strcmp(opt,"jpeg") )
    {
      if (flag_quality >= 0)
        die(i18n(errdupl), opt);
      else if (!arg) 
        flag_quality = 100;
      else 
        {
          flag_quality = strtol(arg,&end,10);
          if (*end || flag_quality<25 || flag_quality>150)
            die(i18n(errbadarg),opt,i18n("an integer between 25 and 150"));
        }
    }
  else if (! strcmp(opt, "help"))
    {
      usage();
    }
  else
    {
      die(i18n("Invalid option '%s'. Try 'ddjvu --help'."), s);
    }
  return i;
}



int
main(int argc, char **argv)
{
  /* Parse options */
  int i;
  for (i=1; i<argc; i++)
    {
      char *s = argv[i];
      if (s[0] == '-' && s[1] != 0)
        i = parse_option(argc, argv, i);
      else if (!inputfilename)
        inputfilename = s;
      else if (! outputfilename)
        outputfilename = s;
      else
        usage();
    }
  
  /* Defaults */
  if (! inputfilename)
    inputfilename = "-";
  if (! outputfilename)
    outputfilename = "-";
  if (! flag_pagespec)
    flag_pagespec = (flag_format) ? "1-$" : "1";

  /* Create context and document */
  if (! (ctx = ddjvu_context_create(argv[0])))
    die(i18n("Cannot create djvu context."));
  if (! (doc = ddjvu_document_create_by_filename(ctx, inputfilename, TRUE)))
    die(i18n("Cannot open djvu document '%s'."), inputfilename);
  while (! ddjvu_document_decoding_done(doc))
    handle(TRUE);
  
  /* Process all pages */
  i = ddjvu_document_get_pagenum(doc);
  parse_pagespec(flag_pagespec, i, dopage);

  /* Close */
#if HAVE_TIFF
  if (tiff)
    {
      if (! TIFFFlush(tiff))
        die(i18n("Error while flushing tiff file."));
      TIFFClose(tiff);
      tiff = 0;
    }
#endif
  if (fout)
    {
      if (fflush(fout) < 0)
        die(i18n("Error while flushing output file: %s"), strerror(errno));
      fclose(fout);
      fout = 0;
    }
  if (doc)
    ddjvu_document_release(doc);
  if (ctx)
    ddjvu_context_release(ctx);
  return 0;
}

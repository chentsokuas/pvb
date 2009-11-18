/****************************************************************************
**
** Copyright (C) 2000-2006 Lehrig Software Engineering.
**
** This file is part of the pvbrowser project.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/
#include "pvdefine.h"
#include <QtCore>
#include <QMessageBox>
#include "opt.h"
#ifdef PVWIN32
#include <windows.h>
#include <direct.h>
#endif

OPT opt;

QString l_file                      = "&File";
QString l_options                   = "&Options";
QString l_new_window                = "&New Window";
QString l_reconnect                 = "&Reconnect";
QString l_save_as_bmp               = "&Save as BMP ...";
QString l_log_as_bmp                = "Log Metafiles as &BMP ...";
QString l_log_as_pvm                = "Log Metafiles as P&VM ...";
QString l_print                     = "&Print";
QString l_new_tab                   = "New &Tab";
QString l_delete_tab                = "Close Tab";
QString l_exit                      = "E&xit";
QString l_edit                      = "&Edit";
QString l_copy                      = "&Copy";
QString l_view                      = "&View";
QString l_editmenu                  = "&Editmenu";
QString l_toolbar                   = "&Toolbar";
QString l_statusbar                 = "&Statusbar";
QString l_maximized                 = "&Maximized";
QString l_fullscreen                = "&Fullscreen";
QString l_help                      = "&Help";
QString l_manual                    = "&Manual";
QString l_about                     = "&About ...";

QString l_status_connection_lost    = "Connection to server lost, CTRL-R for reconnect";
QString l_status_connected          = "Connected to server";
QString l_status_could_not_connect  = "Could not connect to server, CTRL-R for reconnect";
QString l_status_reconnect          = "Reconnect to host";
QString l_status_options            = "Views/changes pvbrowser options";
QString l_status_new_window         = "Opens a new pvbrowser window";
QString l_status_save_as_bmp        = "Saves the actual screen as BMP file";
QString l_status_log_as_bmp         = "Log QDrawWidgets as BMP file";
QString l_status_log_as_pvm         = "Log QDrawWidgets as PVM file";
QString l_status_print              = "Prints the actual screen";
QString l_status_new_tab            = "Opens a new Tab";
QString l_status_exit               = "Closes pvbrowser";
QString l_status_copy               = "Copies the screen to clipboard";
QString l_status_editmenu           = "Enables/disables the editmenu";
QString l_status_toolbar            = "Enables/disables the toolbar";
QString l_status_statusbar          = "Enables/disables the statusbar";
QString l_status_toggle_maximized   = "Maximized On/Off";
QString l_status_toggle_full_screen = "FullScreenMode On/Off";
QString l_status_manual             = "Shows application specific help";
QString l_status_about              = "Shows the aboutbox";

QString l_print_header              = "Printed by pvbrowser at: ";

int wsa()
{
#ifdef PVWIN32
  WORD    wVersionRequested;
  WSADATA wsadata;
  int     err;

#ifdef IS_OLD_MSVCPP
  wVersionRequested = MAKEWORD(1,1);
#else
  wVersionRequested = MAKEWORD(2,0);
#endif
  err = WSAStartup(wVersionRequested, &wsadata);
  if(err != 0)
  {
    printf("Startup error=%d on windows\n",err);
    exit(0);
  }
#endif
  return 0;
}

void getLanguage(const char *bufin)
{
  char *cptr;
  char phrase[MAXOPT],buf[MAXOPT];
  QString qphrase;

  strcpy(buf,bufin);
  cptr = strchr(buf,'='); // get phrase
  cptr++;
  if(*cptr == ' ') cptr ++;
  strcpy(phrase,cptr);
  cptr = strchr(phrase,'\n');
  if(cptr != NULL) *cptr = '\0';
  qphrase = QString::fromUtf8(phrase);

  if     (strncmp(buf,"-print_header",13)              == 0) l_print_header = qphrase;
  else if(strncmp(buf,"-file",5)                       == 0) l_file = qphrase;
  else if(strncmp(buf,"-options",8)                    == 0) l_options = qphrase;
  else if(strncmp(buf,"-new_window",11)                == 0) l_new_window = qphrase;
  else if(strncmp(buf,"-new_tab",8)                    == 0) l_new_tab = qphrase;
  else if(strncmp(buf,"-delete_tab",11)                == 0) l_delete_tab = qphrase;
  else if(strncmp(buf,"-reconnect",10)                 == 0) l_reconnect = qphrase;
  else if(strncmp(buf,"-save_as_bmp",12)               == 0) l_save_as_bmp = qphrase;
  else if(strncmp(buf,"-log_as_bmp",11)                == 0) l_log_as_bmp = qphrase;
  else if(strncmp(buf,"-log_as_pvm",11)                == 0) l_log_as_pvm = qphrase;
  else if(strncmp(buf,"-print",6)                      == 0) l_print = qphrase;
  else if(strncmp(buf,"-exit",5)                       == 0) l_exit = qphrase;
  else if(strncmp(buf,"-edit",5)                       == 0) l_edit = qphrase;
  else if(strncmp(buf,"-toolbar",8)                    == 0) l_toolbar = qphrase;
  else if(strncmp(buf,"-statusbar",10)                 == 0) l_statusbar = qphrase;
  else if(strncmp(buf,"-maximized",10)                 == 0) l_maximized = qphrase;
  else if(strncmp(buf,"-fullscreen",11)                == 0) l_fullscreen = qphrase;
  else if(strncmp(buf,"-view",5)                       == 0) l_view = qphrase;
  else if(strncmp(buf,"-help",5)                       == 0) l_help = qphrase;
  else if(strncmp(buf,"-manual",7)                     == 0) l_manual = qphrase;
  else if(strncmp(buf,"-about",6)                      == 0) l_about = qphrase;
  else if(strncmp(buf,"-status_connection_lost",23)    == 0) l_status_connection_lost = qphrase;
  else if(strncmp(buf,"-status_connected",17)          == 0) l_status_connected = qphrase;
  else if(strncmp(buf,"-status_could_not_connect",25)  == 0) l_status_could_not_connect = qphrase;
  else if(strncmp(buf,"-status_reconnect",17)          == 0) l_status_reconnect = qphrase;
  else if(strncmp(buf,"-status_options",15)            == 0) l_status_options = qphrase;
  else if(strncmp(buf,"-status_new_window",18)         == 0) l_status_new_window = qphrase;
  else if(strncmp(buf,"-status_new_tab",15)            == 0) l_status_new_tab = qphrase;
  else if(strncmp(buf,"-status_save_as_bmp",19)        == 0) l_status_save_as_bmp = qphrase;
  else if(strncmp(buf,"-status_log_as_bmp",18)         == 0) l_status_log_as_bmp = qphrase;
  else if(strncmp(buf,"-status_log_as_pvm",18)         == 0) l_status_log_as_pvm = qphrase;
  else if(strncmp(buf,"-status_print",13)              == 0) l_status_print = qphrase;
  else if(strncmp(buf,"-status_exit",12)               == 0) l_status_exit = qphrase;
  else if(strncmp(buf,"-status_copy",12)               == 0) l_status_copy = qphrase;
  else if(strncmp(buf,"-status_editmenu",16)           == 0) l_status_editmenu = qphrase;
  else if(strncmp(buf,"-status_toolbar",15)            == 0) l_status_toolbar = qphrase;
  else if(strncmp(buf,"-status_statusbar",17)          == 0) l_status_statusbar = qphrase;
  else if(strncmp(buf,"-status_toggle_maximized",24)   == 0) l_status_toggle_maximized = qphrase;
  else if(strncmp(buf,"-status_toggle_full_screen",26) == 0) l_status_toggle_full_screen = qphrase;
  else if(strncmp(buf,"-status_manual",14)             == 0) l_status_manual = qphrase;
  else if(strncmp(buf,"-status_about",13)              == 0) l_status_about = qphrase;
}

const char *inifile()
{
  static char name[MAXOPT];

  if(opt.arg_localini == 1)
  {
    strcpy(name,opt.arg_ini);
    return name;
  }
#ifdef PVUNIX
  strcpy(name,getenv("HOME"));
  strcat(name,"/.pvbrowser.ini");
#endif
#ifdef __VMS
  strcpy(name,"sys$login:pvbrowser.ini");
#endif
#ifdef PVWIN32
  ExpandEnvironmentStrings("%USERPROFILE%",name,sizeof(name)-1);
  if(strcmp(name,"%USERPROFILE%") == 0) strcpy(name,"C:");
  strcat(name,"\\pvbrowser.ini");
#endif
  return name;
}

const char *passfile()
{
  static char name[MAXOPT];

  if(opt.arg_localini == 1)
  {
    strcpy(name,"pvbrowserpass.ini");
    return name;
  }
#ifdef PVUNIX
  strcpy(name,getenv("HOME"));
  strcat(name,"/.pvbrowserpass.ini");
#endif
#ifdef __VMS
  strcpy(name,"sys$login:pvbrowserpass.ini");
#endif
#ifdef PVWIN32
  ExpandEnvironmentStrings("%USERPROFILE%",name,sizeof(name)-1);
  if(strcmp(name,"%USERPROFILE%") == 0) strcpy(name,"C:");
  strcat(name,"\\pvbrowserpass.ini");
#endif
  return name;
}

const char *pvpass(const char *p)
{
  static char ret[4*16+8];
  char buf[80];
  int  i,val;

  ret[0] = '\0';
  for(i=0; p[i] != '\0' && i<=16; i++)
  {
    val = p[i] * 16;
    sprintf(buf,"%04X",val);
    strcat(ret,buf);
  }
  return ret;
}

const char *readIniFile()
{
FILE *fp;
char buf[MAXOPT],buf2[MAXOPT];
const char *cptr;
int i;

  opt.port=5050;
  opt.sshport=50500;
  opt.zoom=100;
  opt.fontzoom=100;
  opt.autoreconnect=0;
  opt.exitpassword=0;
  opt.menubar=1;
  opt.toolbar=1;
  opt.statusbar=1;
  opt.scrollbars=1;
  opt.fullscreen=0;
  opt.maximized=0;
  opt.echo_table_updates=0;
  opt.temp[0] = '\0';
  opt.customlogo[0] = '\0';
  opt.newwindow[0] = '\0';
  opt.ssh[0] = '\0';
  opt.view_pdf[0] = '\0';
  opt.view_img[0] = '\0';
  opt.view_svg[0] = '\0';
  opt.view_txt[0] = '\0';
  opt.view_html[0] = '\0';
  opt.initialhost[0] = '\0';
  opt.language[0] = '\0';
  opt.codec = pvbUTF8; // strongly advised to use pvbUTF8
  opt.closed = 0;
  opt.cookies = 1; // YES
  strcpy(opt.manual,"index.html");

  while(1)
  {
    fp = fopen(inifile(),"r");
    if(fp != NULL)
    {
      while( fgets(buf,sizeof(buf)-1,fp) != NULL )
      {
        if(strncmp(buf,"port=",5) == 0)
        {
          sscanf(buf,"port=%d",&opt.port);
        }
        else if(strncmp(buf,"sshport=",8) == 0)
        {
          sscanf(buf,"sshport=%d",&opt.sshport);
        }
        else if(strncmp(buf,"zoom=",5) == 0)
        {
          sscanf(buf,"zoom=%d",&opt.zoom);
        }
        else if(strncmp(buf,"fontzoom=",9) == 0)
        {
          sscanf(buf,"fontzoom=%d",&opt.fontzoom);
        }
        else if(strncmp(buf,"autoreconnect=",14) == 0)
        {
          sscanf(buf,"autoreconnect=%d",&opt.autoreconnect);
        }
        else if(strncmp(buf,"scrollbars=",11) == 0)
        {
          sscanf(buf,"scrollbars=%d",&opt.scrollbars);
        }
        else if(strncmp(buf,"cookies=",8) == 0)
        {
          sscanf(buf,"cookies=%d",&opt.cookies);
        }
        else if(strncmp(buf,"temp=",5) == 0)
        {
          int ret;
          sscanf(buf,"temp=%s",buf2);
#ifdef PVWIN32
          ExpandEnvironmentStrings(buf2,buf,sizeof(buf)-1);
          if(strstr(buf,"%") != NULL) QMessageBox::warning(NULL,buf,"readIniFile temp directory unknown: adjust pvbrowser.ini temp=");
          strcpy(buf2,buf);
#endif
          ret = chdir(buf2);
          if(ret < 0) 
          {
#ifdef PVUNIX
            QMessageBox::warning(NULL,buf2,"readIniFile could not change to temp dir: adjust ~/.pvbrowser.ini temp=");
            ret = system("xterm -e \"vi ~/.pvbrowser.ini\" &");
#endif
#ifdef PVWIN32
            QMessageBox::warning(NULL,buf2,"readIniFile could not change to temp dir: adjust pvbrowser.ini temp=");
            sprintf(buf,"notepad %s",inifile());
            system(buf);
#endif
            return "Error chdir";
          }
#ifdef PVUNIX
          strcat(buf2,"/");
#endif
#ifdef PVWIN32
          strcat(buf2,"\\");
#endif
          strcpy(opt.temp,buf2);
        }
        else if(strncmp(buf,"toolbar=",8) == 0)
        {
          sscanf(buf,"toolbar=%d",&opt.toolbar);
        }
        else if(strncmp(buf,"statusbar=",10) == 0)
        {
          sscanf(buf,"statusbar=%d",&opt.statusbar);
        }
        else if(strncmp(buf,"menubar=",8) == 0)
        {
          sscanf(buf,"menubar=%d",&opt.menubar);
        }
        else if(strncmp(buf,"initialhost=",12) == 0)
        {
          sscanf(buf,"initialhost=%s",buf2);
          strcpy(opt.initialhost,buf2);
        }
        else if(strncmp(buf,"fullscreen=",11) == 0)
        {
          sscanf(buf,"fullscreen=%d",&opt.fullscreen);
        }
        else if(strncmp(buf,"maximized=",10) == 0)
        {
          sscanf(buf,"maximized=%d",&opt.maximized);
        }
        else if(strncmp(buf,"exitpassword=",13) == 0)
        {
          sscanf(buf,"exitpassword=%d",&opt.exitpassword);
        }
        else if(strncmp(buf,"echo_table_updates=",19) == 0)
        {
          sscanf(buf,"echo_table_updates=%d",&opt.echo_table_updates);
        }
        else if(strncmp(buf,"newwindow=",10) == 0)
        {
          sscanf(buf,"newwindow=%s",buf);
          strcpy(opt.newwindow,buf);
        }
        else if(strncmp(buf,"ssh=",4) == 0)
        {
          sscanf(buf,"ssh=%s",buf);
          strcpy(opt.ssh,buf);
        }
        else if(strncmp(buf,"view.pdf=",9) == 0)
        {
          sscanf(buf,"view.pdf=%s",opt.view_pdf);
        }
        else if(strncmp(buf,"view.img=",9) == 0)
        {
          sscanf(buf,"view.img=%s",opt.view_img);
        }
        else if(strncmp(buf,"view.svg=",9) == 0)
        {
          sscanf(buf,"view.svg=%s",opt.view_svg);
        }
        else if(strncmp(buf,"view.txt=",9) == 0)
        {
          sscanf(buf,"view.txt=%s",opt.view_txt);
        }
        else if(strncmp(buf,"view.csv=",9) == 0)
        {
          sscanf(buf,"view.csv=%s",opt.view_csv);
        }
        else if(strncmp(buf,"view.html=",10) == 0)
        {
          sscanf(buf,"view.html=%s",opt.view_html);
        }
        else if(strncmp(buf,"language =",10) == 0)
        {
          cptr = strchr(buf,'=');
          cptr++;
          if(*cptr == ' ') cptr++;
          sscanf(cptr,"%s",opt.language);
        }
        else if(strncmp(buf,"customlogo=",11) == 0)
        {
          cptr = strchr(buf,'=');
          cptr++;
          if(*cptr == ' ') cptr++;
          sscanf(cptr,"%s",opt.customlogo);
        }
        else if(strncmp(buf,"codec=",6) == 0)
        {
          opt.codec = pvbUTF8;
          if(strstr(buf,"=None") != NULL) opt.codec = pvbNone;
        }
        else if(buf[0] != '#' && strchr(buf,'{') != NULL)
        {
          for(i=0; buf[i] != ' ' && buf[i] != '{'; i++)
          {
            opt.language_section[i] = buf[i];
          }
          opt.language_section[i] = '\0';
        }
        else if(buf[0] == '-' && strcmp(opt.language,opt.language_section) == 0)
        {
          getLanguage(buf);
        }
      }
      fclose(fp);
      return NULL;
    }
    else // write a default initialisation file
    {
      fp = fopen(inifile(),"w");
      if(fp != NULL)
      {
        fprintf(fp,"###############################################\n");
        fprintf(fp,"# This is the default initialization file for\n");
        fprintf(fp,"# ProcessViewBrowser\n");
        fprintf(fp,"###############################################\n");
        fprintf(fp,"port=5050            # default port\n");
        fprintf(fp,"sshport=50500        # local port for ssh connections\n");
        fprintf(fp,"zoom=100             # zoom factor in percent\n");
        fprintf(fp,"fontzoom=100         # zoom factor for fonts in percent\n");
        fprintf(fp,"autoreconnect=0      # 0|1\n");
        fprintf(fp,"exitpassword=0       # 0|1\n");
        fprintf(fp,"menubar=1            # 0|1\n");
        fprintf(fp,"toolbar=1            # 0|1\n");
        fprintf(fp,"statusbar=1          # 0|1\n");
        fprintf(fp,"scrollbars=1         # 0|1\n");
        fprintf(fp,"fullscreen=0         # 0|1\n");
        fprintf(fp,"maximized=0          # 0|1\n");
        fprintf(fp,"cookies=1            # 0=No 1=Yes 2=Ask\n");
        fprintf(fp,"echo_table_updates=0 # 0|1\n");
        fprintf(fp,"# temporary directory\n");
#ifdef PVUNIX
        fprintf(fp,"temp=/tmp\n");
#endif
#ifdef PVWIN32
        //fprintf(fp,"temp=%%PVBDIR%%\\win\\temp\n");
        fprintf(fp,"temp=%%TEMP%%\n");
#endif
#ifdef __VMS
        fprintf(fp,"temp=sys$login:\n");
#endif
        fprintf(fp,"# your custom logo\n");
#ifdef PVUNIX
        fprintf(fp,"customlogo=/opt/pvb/custom.bmp\n");
#endif
#ifdef PVWIN32
        fprintf(fp,"customlogo=%%PVBDIR%%\\custom.bmp\n");
#endif
#ifdef __VMS
        fprintf(fp,"customlogo=[ProcessView.processview.processview]custom.bmp\n");
#endif
        fprintf(fp,"# howto start \"New Window\", can be commented out\n");
        fprintf(fp,"#newwindow=pvbrowser\n");
        fprintf(fp,"# windows users may put their putty.exe here\n");
        fprintf(fp,"ssh=ssh\n");
        fprintf(fp,"# we will automatically connect to this node\n");
        fprintf(fp,"initialhost=pv://localhost\n");

        fprintf(fp,"# define the programs that handle the following file formats\n");
#ifdef  PVWIN32
        fprintf(fp,"view.pdf=\"c:\\path\\to\\acroread\"\n");
        fprintf(fp,"view.img=mspaint\n");
        fprintf(fp,"view.svg=\"c:\\path\\to\\inkscape\"\n");
        fprintf(fp,"view.txt=\"c:\\windows\\system32\\write\"\n");
        fprintf(fp,"view.csv=\"c:\\path\\to\\ooffice\"\n");
        fprintf(fp,"view.html=\"c:\\path\\to\\firefox\"\n");
#elif   defined(PVMAC)
        fprintf(fp,"view.pdf=okular\n");
        fprintf(fp,"view.img=gimp\n");
        fprintf(fp,"view.svg=inkscape\n");
        fprintf(fp,"view.txt=kwrite\n");
        fprintf(fp,"view.csv=ooffice\n");
        fprintf(fp,"view.html=firefox\n");
#elif   defined(PVUNIX)
        fprintf(fp,"view.pdf=okular\n");
        fprintf(fp,"view.img=gimp\n");
        fprintf(fp,"view.svg=inkscape\n");
        fprintf(fp,"view.txt=kwrite\n");
        fprintf(fp,"view.csv=ooffice\n");
        fprintf(fp,"view.html=firefox\n");
#endif

        fprintf(fp,"##################################################################\n");
        fprintf(fp,"#\n");
        fprintf(fp,"# Language settings for ProcessViewBrowser\n");
        fprintf(fp,"# you may input your own language (I only know german and english)\n");
        fprintf(fp,"# example: comment out: language = german\n");
        fprintf(fp,"#          comment in:  #language = default\n");
        fprintf(fp,"#          restart ProcessViewBrowser\n");
        fprintf(fp,"##################################################################\n");
        fprintf(fp,"language = default\n");
        fprintf(fp,"#language = german\n");
        fprintf(fp,"#language = french\n");
        fprintf(fp,"#language = spanish\n");
        fprintf(fp,"#language = italian\n");
        fprintf(fp,"#---------------------------------------\n");
        fprintf(fp,"german {\n");
        fprintf(fp,"-file               = &Datei\n");
        fprintf(fp,"-options            = &Optionen\n");
        fprintf(fp,"-new_window         = &Neues Fenster\n");
        fprintf(fp,"-new_tab            = Neuer &Tab\n");
        fprintf(fp,"-delete_tab         = Schliesst den aktuellen Tab\n");
        fprintf(fp,"-reconnect          = &Reconnect\n");
        fprintf(fp,"-save_as_bmp        = &Speichere als BMP ...\n");
        fprintf(fp,"-log_as_bmp         = Logge Metafiles als &BMP ...\n");
        fprintf(fp,"-log_as_pvm         = Logge Metafiles als P&VM ...\n");
        fprintf(fp,"-print              = &Drucken\n");
        fprintf(fp,"-exit               = Schli&essen\n");
        fprintf(fp,"-edit               = &Bearbeiten\n");
        fprintf(fp,"-copy               = &Kopieren\n");
        fprintf(fp,"-view               = &Ansicht\n");
        fprintf(fp,"-help               = &Hilfe\n");
        fprintf(fp,"-toolbar            = &Werkzeugleiste\n");
        fprintf(fp,"-statusbar          = &Statuszeile\n");
        fprintf(fp,"-maximized          = &Maximiert\n");
        fprintf(fp,"-fullscreen         = &Vollbildmodus\n");
        fprintf(fp,"-Manual             = &Manual\n");
        fprintf(fp,"-about              = &About ...\n");
        fprintf(fp,"-status_connection_lost    = Verbindung zu Server verloren, CTRL-R fuer erneute Verbindung\n");
        fprintf(fp,"-status_connected          = Verbunden mit Server\n");
        fprintf(fp,"-status_could_not_connect  = Konnte nicht mit Server verbinden, CTRL-R fuer erneute Verbindung\n");
        fprintf(fp,"-status_reconnect          = Erneut mit host verbinden\n");
        fprintf(fp,"-status_options            = Ansehen/Ändern der ProcessViewBrowser Optionen\n");
        fprintf(fp,"-status_new_window         = Öffnet ein neues ProcessViewBrowser Fenster\n");
        fprintf(fp,"-status_new_tab            = Öffnet einen neuen Tab\n");
        fprintf(fp,"-status_save_as_bmp        = Speichert den aktuellen Bildschirm als BMP Datei\n");
        fprintf(fp,"-status_log_as_bmp         = Logge QDrawWidgets als BMP Datei\n");
        fprintf(fp,"-status_log_as_pvm         = Logge QDrawWidgets als PVM Datei\n");
        fprintf(fp,"-status_print              = Druckt den aktuellen Bildschirm\n");
        fprintf(fp,"-status_exit               = Schliesst pvbrowser\n");
        fprintf(fp,"-status_copy               = Kopiere den aktuellen Bildschirm in die Zwischenablage\n");
        fprintf(fp,"-status_toolbar            = Werkzeugleiste (an/aus)\n");
        fprintf(fp,"-status_statusbar          = Statuszeile (an/aus)\n");
        fprintf(fp,"-status_toggle_full_screen = Vollbildmodus (an/aus)\n");
        fprintf(fp,"-status_manual             = Anwendungsspezifische Hilfe\n");
        fprintf(fp,"-status_about              = Aboutbox anzeigen\n");
        fprintf(fp,"-print_header              = Gedruckt von pvbrowser am: \n");
        fprintf(fp,"}\n");
        fprintf(fp,"#---------------------------------------\n");
        fprintf(fp,"# Ne pas oublier de dï¿½ommanter ISO-8859-1\n");
        fprintf(fp,"french {\n");
        fprintf(fp,"-file               = Fichier\n");
        fprintf(fp,"-options            = Option\n");
        fprintf(fp,"-new_window         = Nouvelle Fenï¿½re\n");
        fprintf(fp,"-new_tab            = Nouvelle &Tab\n");
        fprintf(fp,"-reconnect          = Reconnection\n");
        fprintf(fp,"-save_as_bmp        = Sauvegarder en BMP ...\n");
        fprintf(fp,"-log_as_bmp         = Enregistrer en BMP ...\n");
        fprintf(fp,"-log_as_pvm         = Enregistrer en PVM ...\n");
        fprintf(fp,"-print              = Imprimer\n");
        fprintf(fp,"-exit               = Sortir\n");
        fprintf(fp,"-edit               = ï¿½iter\n");
        fprintf(fp,"-copy               = Copier\n");
        fprintf(fp,"-view               = Voir\n");
        fprintf(fp,"-toolbar            = Bare d'outil\n");
        fprintf(fp,"-statusbar          = Bare d'ï¿½at\n");
        fprintf(fp,"-toggle_full_screen = Permuter la grandeur de fenï¿½re\n");
        fprintf(fp,"-help               = Aide\n");
        fprintf(fp,"-booklet            = Livret\n");
        fprintf(fp,"-manual             = Manuel\n");
        fprintf(fp,"-about              = ï¿½propos\n");
        fprintf(fp,"-recent_urls        = Dernier sites visitï¿½n");
        fprintf(fp,"-status_connection_lost   = ï¿½at: Connexion perdu, Faire CTRL-R pour reconnecter en local\n");
        fprintf(fp,"-status_connected         = ï¿½at: Connectï¿½n");
        fprintf(fp,"-status_could_not_connect = ï¿½at: Ne peut pas connecter, faire CTRL-R pour reconnecter local\n");
        fprintf(fp,"-status_options     = ï¿½at: Options\n");
        fprintf(fp,"-status_new_window  = ï¿½at: Nouvelle fenï¿½re\n");
        fprintf(fp,"-status_save_as_bmp = ï¿½at: Sauvegardï¿½en BMP\n");
        fprintf(fp,"-status_log_as_bmp  = ï¿½at: QDrawWidgets est enregistrï¿½en BMP\n");
        fprintf(fp,"-status_log_as_pvm  = ï¿½at: QDrawWidgets est enregsitrï¿½en PVM\n");
        fprintf(fp,"-status_print       = ï¿½at: Imprimï¿½n");
        fprintf(fp,"-status_exit        = ï¿½at: Sortie de ProcessViewBrowser\n");
        fprintf(fp,"-status_copy        = ï¿½at: Copiï¿½n");
        fprintf(fp,"-status_toolbar     = ï¿½at: Bare d'outil\n");
        fprintf(fp,"-status_statusbar   = ï¿½at: bare d'ï¿½at\n");
        fprintf(fp,"-status_toggle_full_screen = ï¿½at: Permutï¿½en mode pleinne ï¿½ran\n");
        fprintf(fp,"-status_booklet     = ï¿½at: Livret sur ProcessViewServer\n");
        fprintf(fp,"-status_manual      = ï¿½at: Documentation complï¿½e de ProcessViewServer\n");
        fprintf(fp,"-status_about       = ï¿½at: ï¿½propos\n");
        fprintf(fp,"-print_header       = Imprimer l'entï¿½e. \n");
        fprintf(fp,"}\n");
        fprintf(fp,"#---------------------------------------\n");
        fprintf(fp,"spanish {\n");
        fprintf(fp,"-file               = &Fichero\n");
        fprintf(fp,"-options            = &Opciones\n");
        fprintf(fp,"-new_window         = &Nueva Ventana\n");
        fprintf(fp,"-new_tab            = Nueva &Tab\n");
        fprintf(fp,"-reconnect          = &Reconectar\n");
        fprintf(fp,"-save_as_bmp        = &Guardar como BMP...\n");
        fprintf(fp,"-log_as_bmp         = Log Metafiles como &BMP...\n");
        fprintf(fp,"-log_as_pvm         = Log Metafiles como P&VM...\n");
        fprintf(fp,"-print              = &Imprimir\n");
        fprintf(fp,"-exit               = &Salir\n");
        fprintf(fp,"-edit               = &Editar\n");
        fprintf(fp,"-copy               = &Copiar\n");
        fprintf(fp,"-view               = &Ver\n");
        fprintf(fp,"-toolbar            = &Barra Herramientas\n");
        fprintf(fp,"-statusbar          = &Barra Estado\n");
        fprintf(fp,"-toggle_full_screen = Pantalla Completa (On/Off)\n");
        fprintf(fp,"-help               = &Ayuda\n");
        fprintf(fp,"-booklet            = &Manuales\n");
        fprintf(fp,"-manual             = &Documentación...\n");
        fprintf(fp,"-about              = &Acerca de...\n");
        fprintf(fp,"-recent_urls        = URLs Recientes\n");
        fprintf(fp,"-status_connection_lost   = Conexión al Servidor perdida, CTRL-R para reconecta\n");
        fprintf(fp,"-status_connected         = Conectado al Servidor\n");
        fprintf(fp,"-status_reconnect   = Reconectar al Servidor\n");
        fprintf(fp,"-status_could_not_connect = No se pudo conectar al Servidor, CTRL-R para reconectar\n");
        fprintf(fp,"-status_options     = Ver/Cambiar las Opciones ProcessViewBrowser\n");
        fprintf(fp,"-status_new_window  = Abrir nueva ventana de ProcessViewBrowser\n");
        fprintf(fp,"-status_save_as_bmp = Guardar pantalla actual como fichero BMP\n");
        fprintf(fp,"-status_log_as_bmp  = Logge QDrawWidgets como fichero BMP\n");
        fprintf(fp,"-status_log_as_pvm  = Logge QDrawWidgets como fichero PVM\n");
        fprintf(fp,"-status_print       = Imprimir la pantalla actual\n");
        fprintf(fp,"-status_exit        = Salir de ProcessViewBrowser\n");
        fprintf(fp,"-status_copy        = Copiar la pantalla al Portapapeles\n");
        fprintf(fp,"-status_toolbar     = Barra de Herramientas (On/Off)\n");
        fprintf(fp,"-status_statusbar   = Barra de Estado (On/Off)\n");
        fprintf(fp,"-status_toggle_full_screen = Pantalla Completa (On/Off)\n");
        fprintf(fp,"-status_booklet     = Mostrar ProcessViewServer Manual\n");
        fprintf(fp,"-status_manual      = Mostrar ProcessViewServer Documentación\n");
        fprintf(fp,"-status_about       = Acerca de...\n");
        fprintf(fp,"-print_header       = Impreso desde ProcessViewBrowser\n");
        fprintf(fp,"}\n");
        fprintf(fp,"#---------------------------------------\n");
        fprintf(fp,"Italian {\n");
        fprintf(fp,"-file               = &Dati\n");
        fprintf(fp,"}\n");
        fprintf(fp,"# other languages\n");
        fprintf(fp,"##################################################################\n");
        fprintf(fp,"#\n");
        fprintf(fp,"# End of Language settings for pvbrowser\n");
        fprintf(fp,"#\n");
        fprintf(fp,"##################################################################\n");
        fprintf(fp,"#\n");
        fprintf(fp,"# This is the list of recent nodes you have visited\n");
        fprintf(fp,"host=pv://localhost\n");
        fclose(fp);
      }
      else
      {
        return "Could not write initialization file";
      }
    }
  }
}

int mysystem(const char *command)
{
#ifdef PVWIN32
  int ret;
  STARTUPINFO         si = { sizeof(si)};
  PROCESS_INFORMATION pi;
  char cmd[4096];

  if(strncmp(command,"start",5) == 0 || strncmp(command,"START",5) == 0)
  {
    ExpandEnvironmentStrings(command,cmd,sizeof(cmd)-1);
    ret = system(cmd);
  }
  else
  {
    ExpandEnvironmentStrings(command,cmd,sizeof(cmd)-1);
    ret = (int) CreateProcess( NULL, cmd
                             , NULL, NULL
                             , FALSE, CREATE_NO_WINDOW
                             , NULL, NULL
                             , &si, &pi);
  }
  return ret;
#else
  if(opt.arg_debug) printf("run client command: %s\n", command);
  return system(command);
#endif
}
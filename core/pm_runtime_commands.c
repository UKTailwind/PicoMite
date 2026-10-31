/* Compiler-independent PicoMite command adapters. The command interpreter
 * is part of PicoMite, not the TinyCC frontend. Preserve the original
 * tokenisation, checks, and DEFAULT argument conventions through pm_execute.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "core/pm_runtime.h"
#include "core/tcc/include/picomite/basic.h"
#include "core/tcc/include/picomite/files.h"
#include "core/tcc/include/picomite/audio.h"
#include "core/tcc/include/picomite/framebuffer.h"
#include "core/tcc/include/picomite/input.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#define RT_DEFAULT (-2)
static void rt_command(const char *format, ...) __attribute__((format(printf,1,2)));
static void rt_command(const char *format, ...)
{
    char cmd[STRINGSIZE + 8];
    va_list ap;
    int n;
    va_start(ap, format);
    n = vsnprintf(cmd, sizeof(cmd), format, ap);
    va_end(ap);
    if (n < 0 || n >= STRINGSIZE) error("String too long");
    pm_execute(cmd);
}
static const char *rt_word(const char *s, bool letters)
{
    const char *p;
    if (!s) error("Argument count");
    for (p = s; *p; ++p)
        if (*p == '"' || (letters && !isalnum((unsigned char)*p)))
            error("Invalid argument");
    return s;
}
static char *rt_arg(char out[16], int v)
{
    if (v == RT_DEFAULT) strcpy(out, ",");
    else snprintf(out, 16, ",%d", v);
    return out;
}
static void rt_trim(char *s)
{
    size_t n = strlen(s);
    while (n && s[n - 1] == ',') s[--n] = 0;
}
static void rt_sprite_load(const char *file, int start, int mode)
{
    char a[16], b[16], line[STRINGSIZE];
    snprintf(line, sizeof(line), "\"%s\"%s%s", rt_word(file, false), rt_arg(a,start), rt_arg(b,mode));
    rt_trim(line);
    rt_command("SPRITE LOAD %s", line);
}
static void rt_play_modfile(const char *file) { rt_command("PLAY MODFILE \"%s\"", rt_word(file,false)); }
static void rt_play_modsample(int s, int channel, int volume)
{
    char a[16], line[64];
    snprintf(line,sizeof(line),"PLAY MODSAMPLE %d,%d%s",s,channel,rt_arg(a,volume));
    rt_trim(line); pm_execute(line);
}
static void rt_play_sound(int ch, const char *position, const char *type, double hz, int volume)
{
    char a[16], line[96];
    snprintf(line,sizeof(line),"PLAY SOUND %d,%s,%s,%.10g%s",ch,
             rt_word(position,true),rt_word(type,true),hz,rt_arg(a,volume));
    rt_trim(line); pm_execute(line);
}
static void rt_play_stop_do(void *arg) {
    (void)arg;
    if (CurrentlyPlaying != P_NOTHING) CloseAudio(1);
}
static void rt_play_stop(void) { pm_core_call(rt_play_stop_do, NULL); }
static void rt_play_tone(double left, double right, int duration)
{
    char a[16], line[96];
    snprintf(line,sizeof(line),"PLAY TONE %.10g,%.10g%s",left,right,rt_arg(a,duration));
    rt_trim(line); pm_execute(line);
}
static void rt_play_wav(const char *file) { rt_command("PLAY WAV \"%s\"",rt_word(file,false)); }
static void rt_play_flac(const char *file) { rt_command("PLAY FLAC \"%s\"",rt_word(file,false)); }
static void rt_play_mp3(const char *file) { rt_command("PLAY MP3 \"%s\"",rt_word(file,false)); }
static void rt_play_midifile(const char *file) { rt_command("PLAY MIDIFILE \"%s\"",rt_word(file,false)); }
static void rt_play_pause(void) { pm_execute("PLAY PAUSE"); }
static void rt_play_resume(void) { pm_execute("PLAY RESUME"); }
static void rt_play_next(void) { pm_execute("PLAY NEXT"); }
static void rt_play_previous(void) { pm_execute("PLAY PREVIOUS"); }
static void rt_play_close(void) { pm_execute("PLAY CLOSE"); }
static void rt_play_volume(int left,int right)
{
    char a[16],b[16],line[64];
    snprintf(line,sizeof(line),"PLAY VOLUME %s%s",rt_arg(a,left)+1,rt_arg(b,right));
    rt_trim(line); pm_execute(line);
}
static void rt_play_bbc_sound(int ch,int amplitude,int pitch,int duration)
{ rt_command("PLAY BBC SOUND %d,%d,%d,%d",ch,amplitude,pitch,duration); }
static void rt_play_bbc_envelope(const int e[14])
{
    char b[192]; size_t n; int k;
    if (!e) error("Argument count");
    n = (size_t)snprintf(b,sizeof(b),"PLAY BBC ENVELOPE");
    for (k=0;k<14;k++) {
        int m;
        if(n>=sizeof(b)) error("String too long");
        m=snprintf(b+n,sizeof(b)-n,"%s%d",k?",":" ",e[k]);
        if(m<0||(size_t)m>=sizeof(b)-n) error("String too long");
        n+=(size_t)m;
    }
    pm_execute(b);
}
static char *rt_sound(char *buf,int size)
{
    if(buf && size>0) snprintf(buf,(size_t)size,"%s",PlayingStr[CurrentlyPlaying]);
    return buf;
}
static void rt_load_image(const char *file,int x,int y,int mode)
{
    char a[16],b[16],c[16],line[STRINGSIZE];
    snprintf(line,sizeof(line),"\"%s\"%s%s%s",rt_word(file,false),
            rt_arg(a,x),rt_arg(b,y),rt_arg(c,mode));
    rt_trim(line); rt_command("LOAD IMAGE %s",line);
}
static void rt_load_jpg(const char *file,int x,int y)
{
    char a[16],b[16],line[STRINGSIZE];
    snprintf(line,sizeof(line),"\"%s\"%s%s",rt_word(file,false),rt_arg(a,x),rt_arg(b,y));
    rt_trim(line); rt_command("LOAD JPG %s",line);
}
/* Filesystem statements. For standard C functions use pm_try to translate
 * BASIC errors into -1/NULL, as the old TCC adapter did. */
static void rt_kill(const char *file) { rt_command("KILL \"%s\"",rt_word(file,false)); }
static void rt_rename(const char *from,const char *to)
{ rt_command("RENAME \"%s\" AS \"%s\"",rt_word(from,false),rt_word(to,false)); }
static void rt_copy(const char *from,const char *to)
{ rt_command("COPY \"%s\" TO \"%s\"",rt_word(from,false),rt_word(to,false)); }
static void rt_mkdir(const char *dir) { rt_command("MKDIR \"%s\"",rt_word(dir,false)); }
static void rt_rmdir(const char *dir) { rt_command("RMDIR \"%s\"",rt_word(dir,false)); }
static void rt_chdir(const char *dir) { rt_command("CHDIR \"%s\"",rt_word(dir,false)); }
static void rt_drive(const char *drive) { rt_command("DRIVE \"%s\"",rt_word(drive,false)); }
static char *rt_cwd(char *buf,int size) { return pm_eval_str("CWD$",buf,size); }
static char *rt_dir(const char *pattern,int what,char *buf,int size)
{
    static const char *const kinds[]={"FILE","DIR","ALL"};
    char expr[STRINGSIZE+48];
    if(!pattern) return pm_eval_str("DIR$()",buf,size);
    if(what==RT_DEFAULT) what=0;
    if(what<0||what>2) error("Invalid argument");
    snprintf(expr,sizeof(expr),"DIR$(\"%s\",%s)",rt_word(pattern,false),kinds[what]);
    return pm_eval_str(expr,buf,size);
}
static void rt_exists_file_do(void *ptr)
{
    struct {const char *name; int value;} *a=ptr;
    a->value=ExistsFile((char *)a->name);
    if(a->value==0) {char path[FF_MAX_LFN]={0};int fs;
      if(ExistsDir((char *)a->name,path,&fs)!=0) a->value=-1;}
}
static int rt_exists_file(const char *name)
{ struct {const char *name;int value;} a={name,0};
  pm_core_call(rt_exists_file_do,&a);return a.value; }
static void rt_exists_dir_do(void *ptr)
{ struct {const char *name;int value;} *a=ptr;
  char path[FF_MAX_LFN]={0};int fs;
  a->value=ExistsDir((char *)a->name,path,&fs); }
static int rt_exists_dir(const char *name)
{ struct {const char *name;int value;} a={name,0};
  pm_core_call(rt_exists_dir_do,&a);return a.value; }
static void rt_filesize_do(void *ptr)
{struct {const char *name;long long value;} *a=ptr;a->value=FileSize((char *)a->name);}
static long long rt_filesize(const char *name)
{struct {const char *name;long long value;} a={name,0};pm_core_call(rt_filesize_do,&a);return a.value;}
static void rt_modified_do(void *ptr)
{struct {const char *name;char *out;int n;} *a=ptr;MMInfoModified(a->name,a->out,a->n);}
static char *rt_modified(const char *name,char *out,int size)
{struct {const char *name;char *out;int n;} a={name,out,size};pm_core_call(rt_modified_do,&a);return out;}
static void rt_free_space_do(void *ptr) { *(long long *)ptr=MMInfoDiskSpace(1); }
static long long rt_free_space(void)
{long long a=0;pm_core_call(rt_free_space_do,&a);return a;}
static void rt_disk_size_do(void *ptr) { *(long long *)ptr=MMInfoDiskSpace(0); }
static long long rt_disk_size(void)
{long long a=0;pm_core_call(rt_disk_size_do,&a);return a;}
static char *rt_drive_name(char *buf,int size)
{if(buf && size>0)snprintf(buf,(size_t)size,"%s",MMInfoDriveName());return buf;}

static char rt_fb_letter(int b)
{
    b=toupper((unsigned char)b);
    if(b!='N'&&b!='F'&&b!='L'&&b!='T'&&b!='2') error("Invalid argument");
    return (char)b;
}
static void rt_fb_create(int buffer)
{
    char c=rt_fb_letter(buffer);
    if(c!='F'&&c!='2') error("Invalid argument");
    pm_execute(c=='2'?"FRAMEBUFFER CREATE 2":"FRAMEBUFFER CREATE");
}
static void rt_fb_layer(int transparent)
{ char a[16];rt_command("FRAMEBUFFER LAYER %s",rt_arg(a,transparent)+1); }
static void rt_fb_layer_top(int transparent)
{ char a[16];rt_command("FRAMEBUFFER LAYER TOP %s",rt_arg(a,transparent)+1); }
static void rt_fb_write(int buffer) { rt_command("FRAMEBUFFER WRITE %c",rt_fb_letter(buffer)); }
static void rt_fb_merge(int colour,int mode,int rate)
{
    char a[16],b[16],cmd[64];
    if(mode!=RT_DEFAULT) {
       mode=toupper((unsigned char)mode);
       if(mode!='B'&&mode!='R'&&mode!='A') error("Invalid argument");
       snprintf(b,sizeof(b),",%c",mode);
    }else strcpy(b,",");
    snprintf(cmd,sizeof(cmd),"FRAMEBUFFER MERGE %s%s",rt_arg(a,colour)+1,b);
    if(rate!=RT_DEFAULT)snprintf(cmd+strlen(cmd),sizeof(cmd)-strlen(cmd),",%d",rate);
    rt_trim(cmd);pm_execute(cmd);
}
static void rt_fb_sync(void){pm_execute("FRAMEBUFFER SYNC");}
static void rt_fb_wait(void){pm_execute("FRAMEBUFFER WAIT");}
static void rt_fb_close(int buffer)
{
    if(buffer==RT_DEFAULT)pm_execute("FRAMEBUFFER CLOSE");
    else rt_command("FRAMEBUFFER CLOSE %c",rt_fb_letter(buffer));
}
static void rt_blit(int x1,int y1,int x2,int y2,int w,int h)
{rt_command("BLIT %d,%d,%d,%d,%d,%d",x1,y1,x2,y2,w,h);}
static void rt_blit_read(int n,int x,int y,int w,int h)
{rt_command("BLIT READ %d,%d,%d,%d,%d",n,x,y,w,h);}
static void rt_blit_write(int n,int x,int y,int mode)
{
    char a[16],cmd[64];snprintf(cmd,sizeof(cmd),"BLIT WRITE %d,%d,%d%s",n,x,y,rt_arg(a,mode));
    rt_trim(cmd);pm_execute(cmd);
}
static void rt_blit_load(int n,const char *file,int x,int y,int w,int h)
{
    char a[16],b[16],c[16],d[16],cmd[STRINGSIZE];
    snprintf(cmd,sizeof(cmd),"%d,\"%s\"%s%s%s%s",n,rt_word(file,false),
             rt_arg(a,x),rt_arg(b,y),rt_arg(c,w),rt_arg(d,h));
    rt_trim(cmd);rt_command("BLIT LOAD %s",cmd);
}
static void rt_blit_close(int n) { rt_command("BLIT CLOSE %d",n); }
static void rt_cursor_on(int no,int x,int y,int colour)
{
    char a[16],b[16],c[16],cmd[96];
    if(no==RT_DEFAULT)strcpy(cmd,"GUI CURSOR ON");
    else {
      snprintf(cmd,sizeof(cmd),"GUI CURSOR ON %d%s%s%s",no,
             rt_arg(a,x),rt_arg(b,y),rt_arg(c,colour));
      rt_trim(cmd);
    }
    pm_execute(cmd);
}
static void rt_cursor_off(void){pm_execute("GUI CURSOR OFF");}
static void rt_cursor_hide(void){pm_execute("GUI CURSOR HIDE");}
static void rt_cursor_show(void){pm_execute("GUI CURSOR SHOW");}
static void rt_cursor_colour(int c){rt_command("GUI CURSOR COLOUR %d",c);}
static void rt_cursor_link(int link)
{pm_execute(link?"GUI CURSOR LINK MOUSE":"GUI CURSOR UNLINK MOUSE");}
static void rt_cursor_load(const char *file)
{rt_command("GUI CURSOR LOAD \"%s\"",rt_word(file,false));}
#if defined(PICOMITEVGA) || defined(GUICONTROLS)
static void rt_cursor_move_do(void *ptr)
{int *a=ptr;CursorMoveC(a[0],a[1]);}
#endif
static void rt_cursor_move(int x,int y)
{
#if defined(PICOMITEVGA) || defined(GUICONTROLS)
    int a[]={x,y};pm_core_call(rt_cursor_move_do,a);
#else
    rt_command("GUI CURSOR %d,%d",x,y);
#endif
}

/* libc filesystem aliases preserve the original -1 (no BASIC longjmp)
 * return convention; the translated error remains in MM.ERRMSG. */
typedef struct {const char *a,*b;} rt_fs_args;
static void rt_fs_kill(void *p){rt_fs_args *a=p;rt_kill(a->a);}
static void rt_fs_rename(void *p){rt_fs_args *a=p;rt_rename(a->a,a->b);}
static void rt_fs_mkdir(void *p){rt_fs_args *a=p;rt_mkdir(a->a);}
static void rt_fs_rmdir(void *p){rt_fs_args *a=p;rt_rmdir(a->a);}
static void rt_fs_chdir(void *p){rt_fs_args *a=p;rt_chdir(a->a);}
static int rt_c_remove(const char *s)
{rt_fs_args a={s,NULL};return pm_try(rt_fs_kill,&a)?-1:0;}
static int rt_c_rename(const char *a,const char *b)
{rt_fs_args x={a,b};return pm_try(rt_fs_rename,&x)?-1:0;}
static int rt_c_mkdir(const char *s,unsigned mode)
{(void)mode;rt_fs_args a={s,NULL};return pm_try(rt_fs_mkdir,&a)?-1:0;}
static int rt_c_rmdir(const char *s)
{rt_fs_args a={s,NULL};return pm_try(rt_fs_rmdir,&a)?-1:0;}
static int rt_c_chdir(const char *s)
{rt_fs_args a={s,NULL};return pm_try(rt_fs_chdir,&a)?-1:0;}

/* Shared libc directory wrappers. Directory iteration is a snapshot: a
 * private copy ensures a nested BASIC DIR$() cannot overwrite the listing. */
struct rt_stat { unsigned mode; long size; long long mtime; };
struct rt_dirent { unsigned char d_type; char d_name[256]; };
_Static_assert(FF_MAX_LFN < 256, "dirent d_name");
typedef struct {
    struct rt_dirent current;
    char *list;
    int len, pos;
} rt_directory;
static void rt_cwd_for_try(void *ptr)
{struct {char *buf;int size;} *a=ptr;rt_cwd(a->buf,a->size);}
static char *rt_c_getcwd(char *buf,size_t size)
{
    struct {char *buf;int size;} a={buf,(int)size};
    if(!buf||!size||size>INT_MAX||pm_try(rt_cwd_for_try,&a))return NULL;
    return buf;
}
typedef struct {const char *name;long long value;char *buf;int size;} rt_file_info;
static void rt_exists_file_for_try(void *ptr)
{rt_file_info *a=ptr;a->value=rt_exists_file(a->name);}
static void rt_exists_dir_for_try(void *ptr)
{rt_file_info *a=ptr;a->value=rt_exists_dir(a->name);}
static void rt_filesize_for_try(void *ptr)
{rt_file_info *a=ptr;a->value=rt_filesize(a->name);}
static void rt_modified_for_try(void *ptr)
{rt_file_info *a=ptr;rt_modified(a->name,a->buf,a->size);}
static long long rt_mtime(const char *s)
{
    int y,mo,d,h,mi,se;long long days;
    if(strlen(s)<19)return 0;
    y=atoi(s);mo=atoi(s+5);d=atoi(s+8);h=atoi(s+11);mi=atoi(s+14);se=atoi(s+17);
    if(mo<1||mo>12||d<1)return 0;
    y-=mo<=2;
    {
      int era=y/400,yoe=y-era*400;
      int doy=(153*(mo+(mo>2?-3:9))+2)/5+d-1;
      int doe=yoe*365+yoe/4-yoe/100+doy;
      days=(long long)era*146097+doe-719468;
    }
    return days*86400+h*3600+mi*60+se;
}
static int rt_c_stat(const char *path,struct rt_stat *st)
{
    rt_file_info x={.name=path};char when[32];
    if(!st||!path||pm_try(rt_exists_file_for_try,&x))return -1;
    memset(st,0,sizeof(*st));
    if(x.value==1){
      if(pm_try(rt_filesize_for_try,&x))return -1;
      st->mode=0100000;st->size=(long)x.value;
    }else{
      if(pm_try(rt_exists_dir_for_try,&x)||x.value!=1)return -1;
      st->mode=0040000;
    }
    x.buf=when;x.size=sizeof(when);
    if(pm_try(rt_modified_for_try,&x)==0)st->mtime=rt_mtime(when);
    return 0;
}
typedef struct {const char *pattern,*path;rt_directory *dir;} rt_dir_args;
static void rt_dir_collect(void *ptr)
{
    rt_dir_args *a=ptr;
    char name[FF_MAX_LFN+1],full[FF_MAX_LFN*2+2];
    size_t n=strlen(a->path);
    int cap=0;
    rt_dir(a->pattern,2,name,sizeof(name));
    while(name[0]){
       int k=(int)strlen(name)+2;
       if(a->dir->len+k>cap){
          char *p=pm_runtime_realloc(a->dir->list,(size_t)(cap=cap*2+k+256));
          if(!p)error("Not enough memory");
          a->dir->list=p;
       }
       snprintf(full,sizeof(full),"%s%s%s",a->path,
              n==0||a->path[n-1]=='/'||a->path[n-1]==':'?"":"/",name);
       a->dir->list[a->dir->len]=rt_exists_dir(full)==1?4:8;
       strcpy(a->dir->list+a->dir->len+1,name);
       a->dir->len+=k;
       rt_dir(NULL,2,name,sizeof(name));
    }
}
static void *rt_c_opendir(const char *path)
{
    rt_directory *d;
    rt_dir_args a;
    char pat[FF_MAX_LFN+4];
    size_t n;
    if(!path||(n=strlen(path))+3>FF_MAX_LFN)return NULL;
    d=pm_runtime_calloc(1,sizeof(*d));
    if(!d)return NULL;
    if(!n||!strcmp(path,"."))strcpy(pat,"*"),path="";
    else snprintf(pat,sizeof(pat),"%s%s*",path,
                  path[n-1]=='/'||path[n-1]==':'?"":"/");
    a.pattern=pat;a.path=path;a.dir=d;
    if(pm_try(rt_dir_collect,&a)){
       pm_runtime_free(d->list);pm_runtime_free(d);return NULL;
    }
    return d;
}
static struct rt_dirent *rt_c_readdir(void *dir)
{
    rt_directory *d=dir;
    if(!d||d->pos>=d->len)return NULL;
    d->current.d_type=(unsigned char)d->list[d->pos];
    strcpy(d->current.d_name,d->list+d->pos+1);
    d->pos+=(int)strlen(d->list+d->pos+1)+2;
    return &d->current;
}
static int rt_c_closedir(void *dir)
{
    rt_directory *d=dir;
    if(!d)return -1;
    pm_runtime_free(d->list);
    pm_runtime_free(d);
    return 0;
}

typedef struct { const char *name; const void *addr; } rt_command_symbol;
#define RT_SYM(n) {#n,(const void *)&rt_##n},
#define RT_AS(n,fn) {#n,(const void *)&fn},
static const rt_command_symbol rt_command_symbols[]={
    RT_AS(pm_sprite_load,rt_sprite_load)
    RT_AS(pm_play_modfile,rt_play_modfile)
    RT_AS(pm_play_modsample,rt_play_modsample)
    RT_AS(pm_play_sound,rt_play_sound)
    RT_AS(pm_play_stop,rt_play_stop)
    RT_AS(pm_play_tone,rt_play_tone)
    RT_AS(pm_play_wav,rt_play_wav)
    RT_AS(pm_play_flac,rt_play_flac)
    RT_AS(pm_play_mp3,rt_play_mp3)
    RT_AS(pm_play_midifile,rt_play_midifile)
    RT_AS(pm_play_pause,rt_play_pause)
    RT_AS(pm_play_resume,rt_play_resume)
    RT_AS(pm_play_next,rt_play_next)
    RT_AS(pm_play_previous,rt_play_previous)
    RT_AS(pm_play_close,rt_play_close)
    RT_AS(pm_play_volume,rt_play_volume)
    RT_AS(pm_play_bbc_sound,rt_play_bbc_sound)
    RT_AS(pm_play_bbc_envelope,rt_play_bbc_envelope)
    RT_AS(pm_sound,rt_sound)
    RT_AS(pm_load_image,rt_load_image)
    RT_AS(pm_load_jpg,rt_load_jpg)
    RT_AS(pm_kill,rt_kill)
    RT_AS(pm_rename,rt_rename)
    RT_AS(pm_copy,rt_copy)
    RT_AS(pm_mkdir,rt_mkdir)
    RT_AS(pm_rmdir,rt_rmdir)
    RT_AS(pm_chdir,rt_chdir)
    RT_AS(pm_drive,rt_drive)
    RT_AS(pm_cwd,rt_cwd)
    RT_AS(pm_dir,rt_dir)
    RT_AS(pm_exists_file,rt_exists_file)
    RT_AS(pm_exists_dir,rt_exists_dir)
    RT_AS(pm_filesize,rt_filesize)
    RT_AS(pm_modified,rt_modified)
    RT_AS(pm_free_space,rt_free_space)
    RT_AS(pm_disk_size,rt_disk_size)
    RT_AS(pm_drive_name,rt_drive_name)
    RT_AS(pm_framebuffer_create,rt_fb_create)
    RT_AS(pm_framebuffer_layer,rt_fb_layer)
    RT_AS(pm_framebuffer_layer_top,rt_fb_layer_top)
    RT_AS(pm_framebuffer_write,rt_fb_write)
    RT_AS(pm_framebuffer_merge,rt_fb_merge)
    RT_AS(pm_framebuffer_sync,rt_fb_sync)
    RT_AS(pm_framebuffer_wait,rt_fb_wait)
    RT_AS(pm_framebuffer_close,rt_fb_close)
    RT_AS(pm_blit,rt_blit)
    RT_AS(pm_blit_read,rt_blit_read)
    RT_AS(pm_blit_write,rt_blit_write)
    RT_AS(pm_blit_load,rt_blit_load)
    RT_AS(pm_blit_close,rt_blit_close)
    RT_AS(pm_cursor_on,rt_cursor_on)
    RT_AS(pm_cursor_off,rt_cursor_off)
    RT_AS(pm_cursor_hide,rt_cursor_hide)
    RT_AS(pm_cursor_show,rt_cursor_show)
    RT_AS(pm_cursor_colour,rt_cursor_colour)
    RT_AS(pm_cursor_link,rt_cursor_link)
    RT_AS(pm_cursor_load,rt_cursor_load)
    RT_AS(pm_cursor_move,rt_cursor_move)
    RT_AS(remove,rt_c_remove)
    RT_AS(unlink,rt_c_remove)
    RT_AS(rename,rt_c_rename)
    RT_AS(mkdir,rt_c_mkdir)
    RT_AS(rmdir,rt_c_rmdir)
    RT_AS(chdir,rt_c_chdir)
    RT_AS(getcwd,rt_c_getcwd)
    RT_AS(stat,rt_c_stat)
    RT_AS(opendir,rt_c_opendir)
    RT_AS(readdir,rt_c_readdir)
    RT_AS(closedir,rt_c_closedir)
};
const void *pm_runtime_commands_resolve(const char *name)
{
    unsigned i;
    for(i=0;i<sizeof(rt_command_symbols)/sizeof(rt_command_symbols[0]);++i)
       if(!strcmp(name,rt_command_symbols[i].name)) return rt_command_symbols[i].addr;
    return NULL;
}
unsigned pm_runtime_commands_firmware_id(void)
{
    uint32_t h=2166136261u;unsigned i,j;
    for(i=0;i<sizeof(rt_command_symbols)/sizeof(rt_command_symbols[0]);++i){
       uint32_t v=(uint32_t)(uintptr_t)rt_command_symbols[i].addr;
       for(j=0;j<4;++j,v>>=8)h=(h^(v&255u))*16777619u;
    }
    return h?h:1u;
}

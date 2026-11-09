/* Focused extracted-asset reader contract; reads the reference install, writes only a fresh /tmp folder.
 * Build serially (no core build needed):
 * clang -std=c11 -O1 -Wall -Wextra -Icore/src core/tools/extracted_data_test.c
 *   core/src/fmt_common.c core/src/fmt_vfs.c core/src/fmt_db.c core/src/fmt_bxml.c
 *   core/src/fmt_inflate.c core/src/fmt_lzx.c -o /tmp/rx_extracted_data_test
 * /tmp/rx_extracted_data_test --synthetic-only   (no original assets read)
 * /tmp/rx_extracted_data_test "game files/Archive"   (explicit reference-only mode)
 */
#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "fmt_db.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define REQUIRE(x,label) do { if(!(x)){fprintf(stderr,"FAIL %s: %s\n",label,fmt_error());return 1;}printf("PASS %s\n",label); }while(0)
static int write_path(const char* root,const char* rel,const void* data,size_t size) {
 char p[8192];if(snprintf(p,sizeof p,"%s/%s",root,rel)>=(int)sizeof p)return -1;
 for(char* c=p+1;*c;c++)if(*c=='/'){*c=0;if(mkdir(p,0700)&&errno!=EEXIST)return -1;*c='/';}
 FILE* f=fopen(p,"wb");if(!f)return -1;int bad=fwrite(data,1,size,f)!=size;if(fclose(f))bad=1;return bad?-1:0;
}
static int copy_file(const fmt_vfs* source,const char* root,const char* rel) {
 fmt_buf b={0};if(fmt_vfs_read_file(source,rel,&b))return -1;int rc=write_path(root,rel,b.data,b.size);fmt_buf_free(&b);return rc;
}
/* Independent fixture writer for the documented byte-wise percent encoding. */
static int component(char* out,size_t cap,const char* s) {
 static const char hex[]="0123456789ABCDEF";size_t n=0;
 for(const unsigned char* p=(const unsigned char*)s;*p;p++){
  int plain=(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-';
  if(n+(plain?1:3)>=cap)return -1;
  if(plain)out[n++]=(char)*p;else {out[n++]='%';out[n++]=hex[*p>>4];out[n++]=hex[*p&15];}
 }out[n]=0;return 0;
}
static int asset_path(char* out,size_t cap,const fmt_asset* a) {
 char t[1024],d[1024],p[1024],n[2048];
 if(component(t,sizeof t,a->type)||component(d,sizeof d,a->package->database->name)||component(p,sizeof p,a->package->name)||component(n,sizeof n,a->name))return -1;
 return snprintf(out,cap,"Assets/%s/%s/%s/%s.%s",t,d,p,n,t)>=(int)cap?-1:0;
}
static const fmt_asset* choose_asset(const fmt_database* db) {
 for(int pass=0;pass<2;pass++) {
 for(int p=0;p<db->npackages;p++) {
 for(int j=0;j<db->packages[p].nassets;j++) {
  const fmt_asset* a=db->packages[p].assets+j;
  if(!a->size||a->size>(1u<<20)||a->block_size||a->compressed_other)continue;
  if(!pass&&strcmp(a->type,"material")&&strcmp(a->type,"tbldata")&&strcmp(a->type,"bxml"))continue;
  return a;
 }
 }
 }
 return NULL;
}
/* No installed game / metadata / resource decoding in this mode. */
static int synthetic_only(const char* loose,const char* fallback) {
 const unsigned char sentinel[]={0,0xff,0x25,0x2f,0xc3,0xa9,0};
 const char* escaped="Assets/t%25%2F%2E%2E/%2E%2E%2FDb%5C%25%C3%A9/%2E/%2E%2E%2Fx%252F%5Ca%3Ab.t%25%2F%2E%2E";
 REQUIRE(!write_path(loose,"Database/fixture.txt","synthetic\n",10),"synthetic Database folder");
 REQUIRE(!write_path(loose,"reflex-extracted-v1.txt","1\n",2),"synthetic extracted marker");
 REQUIRE(!write_path(loose,escaped,sentinel,sizeof sentinel),"synthetic traversal bytes");
 REQUIRE(!write_path(loose,"Assets/%4EUL/%63om1/%4CPT9/%43ON.%4EUL",sentinel,sizeof sentinel),"synthetic device-name bytes");
 fmt_database db={0};db.name="../Db\\%\xc3\xa9";fmt_package pkg={0};pkg.name=".";pkg.file="fake";pkg.database=&db;
 fmt_asset a={0};a.name="../x%2F\\a:b";a.type="t%/..";a.package=&pkg;a.size=sizeof sentinel;
 fmt_vfs* v=fmt_vfs_open(loose);REQUIRE(v,"synthetic VFS opened");fmt_buf b={0};
 REQUIRE(!fmt_db_read(v,&a,&b)&&b.size==sizeof sentinel&&!memcmp(b.data,sentinel,b.size),"traversal and UTF-8 exact encoding");fmt_buf_free(&b);
 db.name="com1";pkg.name="LPT9";a.name="CON";a.type="NUL";
 REQUIRE(!fmt_db_read(v,&a,&b)&&b.size==sizeof sentinel&&!memcmp(b.data,sentinel,b.size),"DOS reserved encoding agrees with managed importer");fmt_buf_free(&b);
 char long_name[5001];memset(long_name,'%',5000);long_name[5000]=0;a.name=long_name;
 REQUIRE(fmt_db_read(v,&a,&b)<0&&strstr(fmt_error(),"path too long")&&!b.data&&!b.size,"overlong paths reject empty output");fmt_vfs_close(v);
 REQUIRE(!write_path(fallback,"Database/fake.dx9.package",sentinel,sizeof sentinel)&&!write_path(fallback,"reflex-extracted-v1.txt","1\n",2),"fabricated fallback package and marker");
 a.name="missing";a.type="texture";db.name="Test";pkg.name="Pkg";
 v=fmt_vfs_open(fallback);REQUIRE(v,"synthetic fallback VFS opened");
 REQUIRE(fmt_db_read(v,&a,&b)<0&&strstr(fmt_error(),"extracted asset missing")&&!b.data&&!b.size,"marker forbids package fallback");fmt_vfs_close(v);
 char marker[2048];snprintf(marker,sizeof marker,"%s/reflex-extracted-v1.txt",fallback);REQUIRE(!unlink(marker),"synthetic marker removed");
 v=fmt_vfs_open(fallback);REQUIRE(v,"synthetic unmarked VFS opened");
 REQUIRE(!fmt_db_read(v,&a,&b)&&b.size==sizeof sentinel&&!memcmp(b.data,sentinel,b.size),"unmarked control reads fabricated package");fmt_buf_free(&b);fmt_vfs_close(v);
 /* MODS/TRACKS (fmt_vfs_mount_dir): a mod folder's packed file seen at Database/<name>, read packed even in an extracted install; a mount
  * replaces an install file of the same path (any case); other names and sub folders of the mod folder are not mounted */
 {
  char mod[1200];snprintf(mod,sizeof mod,"%s/../mod track",fallback);
  const unsigned char other[]={7,6,5,4,3,2,1};
  REQUIRE(!write_path(mod,"FAKE.DX9.PACKAGE",other,sizeof other)&&!write_path(mod,"Mods.dx9.package",sentinel,sizeof sentinel)&&!write_path(mod,"sub/fake.dx9.scene",sentinel,sizeof sentinel)
          &&!write_path(fallback,"reflex-extracted-v1.txt","1\n",2),"synthetic mod folder");
  v=fmt_vfs_open(fallback);REQUIRE(v,"synthetic marked VFS opened");
  const int files=fmt_vfs_file_count(v);
  REQUIRE(fmt_vfs_mount_dir(v,"Database",mod,"fake")==1&&fmt_vfs_file_count(v)==files,"mount: one file of the stem, replacing the install's");
  REQUIRE(fmt_vfs_mounted(v,"database/Fake.dx9.package")&&!fmt_vfs_mounted(v,"Database/fixture.txt")&&!fmt_vfs_exists(v,"Database/fake.dx9.scene")&&!fmt_vfs_exists(v,"Database/Mods.dx9.package"),"mount: only the stem's own files");
  a.size=sizeof other;
  REQUIRE(!fmt_db_read(v,&a,&b)&&b.size==sizeof other&&!memcmp(b.data,other,b.size),"mounted package read packed despite the extracted marker");fmt_buf_free(&b);
  REQUIRE(fmt_vfs_mount_dir(v,"Database",mod,"Mods")==1&&fmt_vfs_file_count(v)==files+1&&fmt_vfs_exists(v,"Database/MODS.dx9.package"),"mount: a new path is added");
  REQUIRE(fmt_vfs_mount_dir(v,"Database","/nonexistent/mod folder","fake")<0,"mount: a missing folder fails");
  fmt_vfs_close(v);
 }
 puts("PASS synthetic-only extracted contract; no original install opened");return 0;
}
int main(int argc,char** argv) {
 if(argc!=2)return 2;
 char temp[]="/tmp/rx_extracted_data_XXXXXX";REQUIRE(mkdtemp(temp),"fresh test folder");
 char loose[1024],fallback[1024];snprintf(loose,sizeof loose,"%s/loose",temp);snprintf(fallback,sizeof fallback,"%s/fallback",temp);
 REQUIRE(!mkdir(loose,0700)&&!mkdir(fallback,0700),"isolated asset roots");
 if(!strcmp(argv[1],"--synthetic-only"))return synthetic_only(loose,fallback);
 fmt_install* original=fmt_install_open(argv[1]);REQUIRE(original,"reference install opened");
 const char* names[3]={"MXTables_Default","Rider_Skin_Materials","MX_Handlebars"};
 const fmt_asset* assets[3];fmt_buf decoded[3]={{0}};char paths[3][8192];
 for(int i=0;i<3;i++) {
  const fmt_database* db=fmt_install_db(original,names[i]);REQUIRE(db,"reference database parsed");
  assets[i]=choose_asset(db);REQUIRE(assets[i],"bounded real asset selected");
  REQUIRE(!fmt_install_read(original,assets[i],decoded+i),"original package decoded");
  REQUIRE(!asset_path(paths[i],sizeof paths[i],assets[i]),"extracted path encoded");
  REQUIRE(!write_path(loose,paths[i],decoded[i].data,decoded[i].size),"exact decompressed asset written");
  char metadata[1024];snprintf(metadata,sizeof metadata,"Database/%s.dx9.database",names[i]);
  REQUIRE(!copy_file(fmt_install_vfs(original),loose,metadata),"original metadata retained");
  printf("asset db=%s package=%s name=%s type=%s packed=%u decoded=%zu lzx=%u\n",names[i],assets[i]->package->name,assets[i]->name,assets[i]->type,assets[i]->size,decoded[i].size,assets[i]->lzx);
 }
 REQUIRE(!write_path(loose,"reflex-extracted-v1.txt","1\n",2),"extracted marker written");
 fmt_install* extracted=fmt_install_open(loose);REQUIRE(extracted,"package-free extracted install opened");
 for(int i=0;i<3;i++) {
  fmt_buf b={0};const fmt_asset* a=assets[i];
  REQUIRE(!fmt_install_read_named(extracted,names[i],a->package->name,a->name,a->type,&b),"asset read through retained metadata");
  REQUIRE(b.size==decoded[i].size&&!memcmp(b.data,decoded[i].data,b.size),"packed and loose decoded bytes identical");fmt_buf_free(&b);
 }
 for(int i=0;i<fmt_vfs_file_count(fmt_install_vfs(extracted));i++)REQUIRE(!strstr(fmt_vfs_file_path(fmt_install_vfs(extracted),i),".package"),"extracted root has no original packages");
 fmt_install_close(extracted);
 /* A real original package is present, with matching metadata, but the marker forbids fallback. */
 char metadata[1024],package[1024];snprintf(metadata,sizeof metadata,"Database/%s.dx9.database",names[0]);snprintf(package,sizeof package,"Database/%s.dx9.package",assets[0]->package->file);
 REQUIRE(!copy_file(fmt_install_vfs(original),fallback,metadata)&&!copy_file(fmt_install_vfs(original),fallback,package)&&!write_path(fallback,"reflex-extracted-v1.txt","1\n",2),"missing-loose fixture includes original package");
 fmt_install* missing=fmt_install_open(fallback);REQUIRE(missing,"missing-loose install opened");fmt_buf missing_bytes={0};
 REQUIRE(fmt_vfs_exists(fmt_install_vfs(missing),package),"original fallback package is physically present");
 REQUIRE(fmt_install_read_named(missing,names[0],assets[0]->package->name,assets[0]->name,assets[0]->type,&missing_bytes)<0&&strstr(fmt_error(),"extracted asset missing"),"missing loose asset fails without package fallback");
 REQUIRE(!missing_bytes.data&&!missing_bytes.size,"missing read returns empty output");fmt_install_close(missing);
 char marker[2048];snprintf(marker,sizeof marker,"%s/reflex-extracted-v1.txt",fallback);REQUIRE(!unlink(marker),"marker removed for reference control");
 missing=fmt_install_open(fallback);REQUIRE(missing,"unmarked control opened");REQUIRE(!fmt_install_read_named(missing,names[0],assets[0]->package->name,assets[0]->name,assets[0]->type,&missing_bytes),"same copied package succeeds only without extracted marker");
 REQUIRE(missing_bytes.size==decoded[0].size&&!memcmp(missing_bytes.data,decoded[0].data,missing_bytes.size),"reference fallback control bytes match");fmt_buf_free(&missing_bytes);fmt_install_close(missing);
 /* Traversal, literal percent sequences, Windows separators, colon, UTF-8, dot-only components. */
 const char* encoded="Assets/t%25%2F%2E%2E/%2E%2E%2FDb%5C%25%C3%A9/%2E/%2E%2E%2Fx%252F%5Ca%3Ab.t%25%2F%2E%2E";
 const unsigned char sentinel[]={0,0xff,0x25,0x2f,0xc3,0xa9,0};
 REQUIRE(!write_path(loose,"Assets/%4EUL/%63om1/%4CPT9/%43ON.%4EUL",sentinel,sizeof sentinel),"Windows device-name fixture written");
 REQUIRE(!write_path(loose,encoded,sentinel,sizeof sentinel),"independently encoded traversal fixture written");
 fmt_database db={0};db.name="../Db\\%\xc3\xa9";fmt_package pkg={0};pkg.name=".";pkg.file="absent";pkg.database=&db;
 fmt_asset special={0};special.name="../x%2F\\a:b";special.type="t%/..";special.package=&pkg;special.size=1;
 fmt_vfs* v=fmt_vfs_open(loose);REQUIRE(v,"synthetic extracted VFS opened");fmt_buf got={0};
 REQUIRE(!fmt_db_read(v,&special,&got)&&got.size==sizeof sentinel&&!memcmp(got.data,sentinel,sizeof sentinel),"escaped traversal and UTF-8 identifiers resolve exact flat component");fmt_buf_free(&got);
 db.name="com1";pkg.name="LPT9";special.name="CON";special.type="NUL";
 REQUIRE(!fmt_db_read(v,&special,&got)&&got.size==sizeof sentinel&&!memcmp(got.data,sentinel,sizeof sentinel),"Windows reserved device names match managed segment encoder");fmt_buf_free(&got);
 char overlong[5001];memset(overlong,'%',5000);overlong[5000]=0;special.name=overlong;
 REQUIRE(fmt_db_read(v,&special,&got)<0&&strstr(fmt_error(),"path too long"),"overlong encoded path rejected");REQUIRE(!got.data&&!got.size,"rejected path returns empty output");fmt_vfs_close(v);
 for(int i=0;i<3;i++)fmt_buf_free(decoded+i);fmt_install_close(original);
 printf("PASS extracted asset contract complete; fixtures retained at %s\n",temp);return 0;
}

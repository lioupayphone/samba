/* 
   Unix SMB/CIFS implementation.
   case insensitive test cases
   Copyright (C) Volker Lendecke 2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "torture/torture.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "system/time.h"
#include "system/filesys.h"
#include "system/dir.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "lib/events/events.h"
#include "param/param.h"
#include "torture/raw/proto.h"
#include "lib/util/dlinklist.h"


struct dir_entry_node{
       struct dir_entry_node *next, *prev;
       char *entry_name;
};

struct tranverse_state{
       struct smbcli_tree *tree;
       struct torture_context *context;
};

#define SETUP_REQUEST(cmd, wct, buflen) do { \
        req = smbcli_request_setup(tree, cmd, wct, buflen); \
        if (!req) return NULL; \
} while (0)


#define CHECK_STATUS(torture, status, correct) do {       \
       if (!NT_STATUS_EQUAL(status, correct)) { \
              torture_result(torture, TORTURE_FAIL, "%s: Incorrect status %s - should be %s\n", \
                     __location__, nt_errstr(status), nt_errstr(correct)); \
              ret = false; \
       } \
} while (0)
#define CHECK_VALUE(v, correct) do { \
	if ((v) != (correct)) { \
		torture_fail(torture, talloc_asprintf(torture, "(%s) Incorrect value %s=%d - should be %d\n", \
		       __location__, #v, v, correct)); \
		ret = false; \
		goto done; \
	}} while (0)

static void count_fn(struct clilist_file_info *info, const char *name,
                   void *private_data)
{
       int *counter = (int *)private_data;
       *counter += 1;
}


void  gen_length_string(int length, bool same_case, char *dst)
{
        int i = 0;
        for(i=0; i<length - 1; i++){
            if(true == same_case){
                dst[i] = 'A' + i%26;
                continue;
            }
            if(0 == i%2){
                dst[i] = 'A' + i%26;
            }else{
                dst[i] = 'a' + i%26;
            }
        }
}
void string_cat(char *dst, char *str)
{
       char *p;
       for(p=dst; *p != '\0'; p++);
       while(*str != '\0'){
           *p = *str;
           p++;
           str++;
       }
       *p = '\0';
}
void  gen_chinese_string(int cnt, char *dst)
{
       int i = 0;
       int index = 0;
       char chinese_words[5][4] = {
              {0xE6, 0x98, 0xA5, '\0'},
              {0xE6, 0xB5, 0x8B, '\0'},
              {0xE8, 0xAF, 0x95, '\0'},
              {0xE6, 0x9C, 0xBA, '\0'},
              {0xE6, 0xB1, 0x89, '\0'}
       };
       
       for(i=0; i<cnt; i++){
              srand((unsigned)time(NULL));
              index = rand()%5;
              string_cat(dst, chinese_words[index]);
       }

}
void set_case_sensitive(struct smbcli_state *cli, bool case_sensitive)
{
       uint32_t fs_attrs;
       struct smbXcli_tcon *tcon = cli->tree->smbXcli;
	if (case_sensitive) {
		fs_attrs |= FILE_CASE_SENSITIVE_SEARCH;
	} else {
		fs_attrs &= ~FILE_CASE_SENSITIVE_SEARCH;
	}
       smbXcli_tcon_set_fs_attributes(tcon, fs_attrs);
}

bool create_largedir_case_insensitive(struct torture_context *torture,  struct smbcli_state *cli, const char *root_dir, int level, int level_cnt)
{
       TALLOC_CTX *mem_ctx;
       int i,j;
       char *fpath;
       struct dir_entry_node *list = NULL; 
       struct dir_entry_node *head,*tail,*tmp;
       bool ret = true;
       NTSTATUS status;
       int fnum;
       struct dir_entry_node *test_tail = NULL;

       if(!(mem_ctx = talloc_init("create largedir"))){
               torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
               return false;
       }
       if (!(head=talloc_zero(mem_ctx,  struct dir_entry_node))){
               torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
               ret = false;
               goto done;;
        }
       head->entry_name = talloc_strdup(mem_ctx, root_dir);
       if(! head->entry_name){
               torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
               ret = false;
               goto done;
       }
       DLIST_ADD(list, head);
       tail = head;
       for(i=1; i<level; i++){
               head = NULL;
               while(head != tail){
                      DLIST_HEAD(list, head);
                      for(j=0; j<level_cnt; j++){
                               fpath = talloc_asprintf(mem_ctx, "%s\\Level_%d_DiR_%d", head->entry_name, i, j);
                               status = smbcli_mkdir(cli->tree, fpath);
                               CHECK_STATUS(torture, status, NT_STATUS_OK);
                               if (false == ret)
                                       goto done;
                               if(!(tmp=talloc_zero(mem_ctx, struct dir_entry_node))){
                                       torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
                                       ret = false;
                                       goto done;
                               }
                               if(!(tmp->entry_name = talloc_strdup(mem_ctx, fpath))){
                                       torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
                                       ret = false;
                                       goto done;
                               }
                              
                               DLIST_ADD_END(list, tmp);
                             if(!(fpath = talloc_asprintf(mem_ctx, "%s\\Level_%d_FiLe_%d", head->entry_name, i, j))){
                                       ret = false;
                                       goto done;
                             }
                             fnum = smbcli_open(cli->tree, fpath, O_RDWR | O_CREAT, DENY_NONE);
                             if (fnum == -1){
                                        ret = false;
                                        goto done;
                             }
                             smbcli_close(cli->tree, fnum);
                      }
                      DLIST_REMOVE(list, head);
               }
               tail =  DLIST_TAIL(list);            
               continue;
       }
      
done:       
       talloc_free(mem_ctx);
       return ret;
}


static void tranverse_fn(struct clilist_file_info *finfo, const char *name, void *state)
{
       struct tranverse_state *dstate = (struct tranverse_state *)state;
       char *s, *n;
       if (ISDOT(finfo->name) || ISDOTDOT(finfo->name)) {
              return;
       }

       n = strdup(name);
       n[strlen(n)-1] = 0;
       if (asprintf(&s, "%s%s", n, finfo->name) < 0) {
              free(n);
              return;
       }
    
       if (finfo->attrib & FILE_ATTRIBUTE_DIRECTORY) {
              char *s2;
              if (asprintf(&s2, "%s\\*", s) < 0) {
                     free(s);
                     free(n);
                     return;
               }
              torture_comment(dstate->context, "%s\\%s\n", name, finfo->name);
              smbcli_list(dstate->tree, s2, 
                      FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM, 
                      tranverse_fn, state);
              free(s2);
              
       } else {
              torture_comment(dstate->context, "%s\\%s\n", name, finfo->name);
       }
       free(s);
       free(n);       
}
bool tranverse_largedir_case_insensitive(struct torture_context *torture, struct smbcli_state *cli, const char *root_dir)
{
       TALLOC_CTX *mem_ctx;
       struct tranverse_state tranverse_state;
       bool ret = true;
       
       if(!(mem_ctx = talloc_init("tranverse_largedir_case_insensitive"))){
               torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
               return false;
       }

       tranverse_state.tree = cli->tree;
       tranverse_state.context = torture;
       smbcli_list(cli->tree, talloc_asprintf(
                         mem_ctx, "%s\\*", root_dir),
                  FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN
                  |FILE_ATTRIBUTE_SYSTEM,
                  tranverse_fn, (void *)&tranverse_state);
done:
       talloc_free(mem_ctx);
       return ret;
}

/*
test normal file operation
1. create a file in lower format
2. list the file
3. create same name file in upper format
4. rename the file in upper format
5. unlink the file in lower format
*/
bool test_file_case_insensitive(struct torture_context *torture, struct smbcli_state *cli)
{
       TALLOC_CTX *mem_ctx;
       const char *dirname = "insensitive";
       const char *ucase_dirname = "InSeNsItIvE";
       const char *fname = "foo";
       const char *fname_upper = "FOO";
       char *fpath = NULL;
       char *fpath_dst = NULL;
       int fnum = -1;
       int counter = 0;
       bool ret = true;
       NTSTATUS status;
       union smb_open io;

       if (!(mem_ctx = talloc_init("test_file_case_insensitive"))) {
              torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
              return false;
       }

       torture_assert(torture, torture_setup_dir(cli, dirname), "creating test directory");

       if (!(fpath = talloc_asprintf(mem_ctx, "%s\\%s", dirname, fname))) {
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed.\n");
              ret = false;
              goto done;
       }
       
       torture_comment(torture, "creating file.\n");
       fnum = smbcli_open(cli->tree, fpath, O_RDWR | O_CREAT, DENY_NONE);
       if (fnum == -1) {
              torture_result(torture, TORTURE_FAIL,
                     "Could not create file %s: %s", fpath,
                      smbcli_errstr(cli->tree));
              ret = false;
              goto done;
       }
       smbcli_close(cli->tree, fnum);

       
       torture_comment(torture, "listing file.\n");
       smbcli_list(cli->tree, talloc_asprintf(
                         mem_ctx, "%s\\*", ucase_dirname),
                  FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN
                  |FILE_ATTRIBUTE_SYSTEM,
                  count_fn, (void *)&counter);

       if (counter == 3) {
              ret = true;
       }
       else {
              torture_result(torture, TORTURE_FAIL,
                     "expected 3 entries, got %d", counter);
              ret = false;
              goto done;
       }
     
       torture_comment(torture, "creating file in upper format.\n");
       if(!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s", ucase_dirname, fname_upper))){
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed.\n");
              ret = false;
              goto done;
       }
       io.generic.level = RAW_OPEN_NTCREATEX;
       io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
       io.ntcreatex.in.root_fid.fnum = 0;
       io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
       io.ntcreatex.in.alloc_size = 1024*1024;
       io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
       io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
       io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
       io.ntcreatex.in.create_options = 0;
       io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
       io.ntcreatex.in.security_flags = 0;
       io.ntcreatex.in.fname = fpath_dst;
       status = smb_raw_open(cli->tree, torture, &io);
       CHECK_STATUS(torture, status, NT_STATUS_OBJECT_NAME_COLLISION);
       if(false == ret){
              goto done;
       }
                   
       
       torture_comment(torture, "renaming file.\n");
       if (!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s", dirname, fname_upper))) {
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed.\n");
              ret = false;
              goto done;
       }
       status = smbcli_rename(cli->tree, fpath,  fpath_dst);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
              goto done;

       
       torture_comment(torture, "unlinking file.\n");
       status = smbcli_unlink(cli->tree, fpath);
       CHECK_STATUS(torture, status, NT_STATUS_OK);

 done:
       talloc_free(mem_ctx);
       return ret;
}

/*
normal directory operation
1. create directory in lower format
2. check the path
3. create the same name directory in upper format
4. rename the path 
5. remove the path
*/
bool test_dir_case_insensitive(struct torture_context *torture, struct smbcli_state *cli)
{
       TALLOC_CTX *mem_ctx;
       const char *dirname = "insensitive";
       const char *ucase_dirname = "InSeNsItIvE";
       const char *sub_dirname = "foo_dir";
       const char *sub_dir_name_upper = "FOO_DIR";
       char *fpath = NULL;
       char *fpath_dst = NULL;
       int fnum = -1;
       int counter = 0;
       bool ret = true;
       NTSTATUS status;

       if (!(mem_ctx = talloc_init("test_dir_case_insensitive"))) {
              torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
              return false;
       }

       torture_assert(torture, torture_setup_dir(cli, dirname), "creating test directory");

       if (!(fpath = talloc_asprintf(mem_ctx, "%s\\%s", dirname, sub_dirname))) {
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed\n");
              ret = false;
              goto done;
       }

       
       torture_comment(torture, "creating directory.\n");
       status = smbcli_mkdir(cli->tree, fpath);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
              goto done;

       torture_comment(torture, "checking path.\n");
       status = smbcli_chkpath(cli->tree, talloc_asprintf(mem_ctx, "%s\\%s", ucase_dirname, sub_dir_name_upper));
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
              goto done;

       
       torture_comment(torture, "creating directory in upper format.\n");
       if(!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s", ucase_dirname, sub_dir_name_upper))){
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed\n");
              ret = false;
              goto done;
       }
       status = smbcli_mkdir(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status, NT_STATUS_OBJECT_NAME_COLLISION);
       if (false == ret)
              goto done;

       
       torture_comment(torture, "renaming directory.\n");
       if(!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s", dirname, sub_dir_name_upper))){
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed\n");
              ret = false;
              goto done;
       }
       status = smbcli_rename(cli->tree, fpath, fpath_dst);
       CHECK_STATUS(torture, status,  NT_STATUS_OK);
       if(false == ret)
              goto done;

       
       torture_comment(torture, "removing directory.\n");
       status = smbcli_rmdir(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status,  NT_STATUS_OK);
       
done:
       talloc_free(mem_ctx);
       return ret;
}

/*
file and dir mix operation
1. create a file in lower format
2. create dir that has same name as file in upper format
3. unlink the file
4. create a dir in lower format
5. create a file that has same name as dir in upper format
6. remove the dir
*/
bool test_file_dir_case_insensitive(struct torture_context *torture, struct smbcli_state *cli)
{
       TALLOC_CTX *mem_ctx;
       const char *dirname = "insensitive";
       const char *ucase_dirname = "InSeNsItIvE";
       const char *fname = "foo";
       const char *fname_upper = "FOO";
       const char *sub_dirname = "foo_dir";
       const char *sub_dir_name_upper = "FOO_DIR";
       char *fpath = NULL;
       char *fpath_dst = NULL;
       int fnum;
       int counter = 0;
       bool ret = true;
       NTSTATUS status;   

       if (!(mem_ctx = talloc_init("test_file_dir_case_insensitive"))) {
              torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
              return false;
       }

       torture_assert(torture, torture_setup_dir(cli, dirname), "creating test directory");

       torture_comment(torture, "creating file.\n");
       if (!(fpath = talloc_asprintf(mem_ctx, "%s\\%s", dirname, fname))){
               torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed\n");
              ret = false;
              goto done;
       }
       fnum = smbcli_open(cli->tree, fpath, O_RDWR | O_CREAT, DENY_NONE);
       if (fnum == -1) {
              torture_result(torture, TORTURE_FAIL,
                     "Could not create file %s: %s", fpath,
                      smbcli_errstr(cli->tree));
              ret = false;
              goto done;
       }
       smbcli_close(cli->tree, fnum);
    
       
       torture_comment(torture, "create dir that has same name as file.\n");
       if (!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s", ucase_dirname, fname_upper))){
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed\n");
              ret = false;
              goto done;
       }
       status = smbcli_mkdir(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status, NT_STATUS_OBJECT_NAME_COLLISION);
       if (false == ret)
              goto done;
       
       torture_comment(torture, "unlinking file.\n");
       status = smbcli_unlink(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
              goto done;
       
       torture_comment(torture, "creating directory.\n");
       if(!(fpath = talloc_asprintf(mem_ctx, "%s\\%s", dirname, sub_dirname))){
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed\n");
              ret = false;
              goto done;
       }
       status = smbcli_mkdir(cli->tree, fpath);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
              goto done;
       
       torture_comment(torture, "create file that has same name as dir.\n");
       if(!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s",  dirname, sub_dir_name_upper))){
              ret = false;
              goto done;
       }
       fnum = smbcli_open(cli->tree, fpath_dst, O_RDWR | O_CREAT, DENY_NONE);
       if (fnum != -1){
              ret = false;
              smbcli_close(cli->tree, fnum);
              goto done;
       }
      
       torture_comment(torture, "removing directory.\n");
       status = smbcli_rmdir(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status,  NT_STATUS_OK);
          
done:
       talloc_free(mem_ctx);
       return ret;
}

/*
special character file name test
1. create a file with special character filename
2. list the file
3. rename the file
4. unlink the file
*/
bool test_special_character_filename_case_insensitive(struct torture_context *torture, struct smbcli_state *cli)
{
       TALLOC_CTX *mem_ctx;
       const char *dirname = "insensitive";
       const char *ucase_dirname = "InSeNsItIvE";
       const char *fname = "foo~!@#$%^&()-=_+{}[]`";
       const char *fname_upper = "FOO~!@#$%^&()-=_+{}[]`";
       char *fpath = NULL;
       char *fpath_dst = NULL;
       int fnum;
       int counter = 0;
       bool ret = false;
       NTSTATUS status;

       if (!(mem_ctx = talloc_init("test_special_character"))) {
              torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
              return false;
       }

       torture_assert(torture, torture_setup_dir(cli, dirname), "creating test directory");

       if (!(fpath = talloc_asprintf(mem_ctx, "%s\\%s", dirname, fname))) {
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed\n");
              ret = false;
              goto done;
       }
      
       torture_comment(torture, "creating file.\n");
       fnum = smbcli_open(cli->tree, fpath, O_RDWR | O_CREAT, DENY_NONE);
       if (fnum == -1) {
              torture_result(torture, TORTURE_FAIL,
                     "Could not create file %s: %s", fpath,
                      smbcli_errstr(cli->tree));
              goto done;
       }
       smbcli_close(cli->tree, fnum);

     
       torture_comment(torture, "listing file.\n");
       smbcli_list(cli->tree, talloc_asprintf(
                         mem_ctx, "%s\\*", ucase_dirname),
                  FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN
                  |FILE_ATTRIBUTE_SYSTEM,
                  count_fn, (void *)&counter);

       if (counter == 3) {
              ret = true;
       }
       else {
              torture_result(torture, TORTURE_FAIL,
                     "expected 3 entries, got %d", counter);
              ret = false;
              goto done;
       }
       
       torture_comment(torture, "renaming file.\n");
       if (!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s", dirname, fname_upper))) {
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed\n");
              ret = false;
              goto done;
       }
       status = smbcli_rename(cli->tree, fpath,  fpath_dst);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
              goto done;

       torture_comment(torture, "unlinking file.\n");
       status = smbcli_unlink(cli->tree, fpath);
       CHECK_STATUS(torture, status, NT_STATUS_OK);

 done:
       talloc_free(mem_ctx);
       return ret;
       
}
/*
chinese filename test
1. create a file with chinese and english mix filename
2. list the file
3. unlink the file
4. create a dir with chinese and english mix filename
5. check the path
6. remove the dir
*/
bool test_chinese_case_insensitive(struct torture_context *torture, struct smbcli_state *cli)
{
       TALLOC_CTX *mem_ctx;
       const char *dirname = "insensitive";
       const char *ucase_dirname = "InSeNsItIvE";
       char *fname = "foo";
       char *fname_upper = "FOO";
       char chinese[256] = {'\0'};
       char *fpath;
       char *fpath_dst;
       int fnum;
       int counter = 0;
       int chinese_cnt = 1;
       bool ret = false;
       NTSTATUS status;

       if (!(mem_ctx = talloc_init("test_chinese_case_insensitive"))) {
              torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
              return false;
       }  
       torture_assert(torture, torture_setup_dir(cli, dirname), "creating test directory");
       gen_chinese_string(chinese_cnt, chinese);
       
       if(!(fpath = talloc_asprintf(mem_ctx, "%s\\%s%s", dirname,fname, chinese))){
               ret = false;
               goto done;
       }

       torture_comment(torture, "creating file.\n");
       fnum = smbcli_open(cli->tree, fpath, O_RDWR | O_CREAT, DENY_NONE);
       if (fnum == -1) {
              torture_result(torture, TORTURE_FAIL,
                     "Could not create file %s: %s", fpath,
                      smbcli_errstr(cli->tree));
              goto done;
       }
       smbcli_close(cli->tree, fnum);

       
       torture_comment(torture, "listing file.\n");
       smbcli_list(cli->tree, talloc_asprintf(
                         mem_ctx, "%s\\*", ucase_dirname),
                  FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN
                  |FILE_ATTRIBUTE_SYSTEM,
                  count_fn, (void *)&counter);

       if (counter == 3) {
              ret = true;
       }
       else {
              torture_result(torture, TORTURE_FAIL,
                     "expected 3 entries, got %d", counter);
              ret = false;
              goto done;
       }
       
       torture_comment(torture, "unlinking file.\n");
       if (!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s%s", ucase_dirname, fname_upper, chinese))){
                ret = false;
                goto done;
       }
       status = smbcli_unlink(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
              goto done;  
       
       torture_comment(torture, "creating directory.\n");
       status = smbcli_mkdir(cli->tree, fpath);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
              goto done;

       
        torture_comment(torture, "chking path.\n");
        status = smbcli_chkpath(cli->tree, fpath_dst);
        CHECK_STATUS(torture, status, NT_STATUS_OK);
        if (false == ret)
              goto done;
        
       torture_comment(torture, "removing directory.\n");
       status = smbcli_rmdir(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status,  NT_STATUS_OK);
 
  done:
       talloc_free(mem_ctx);
       return ret;
       
}

/*
mix file name test
1. create a file with chinese , english and special character mix filename
2. list the file
3. unlink the file
4. create a dir with chinese , english and special character mix filename
5. check the path
6. remove the dir
*/
bool test_mix_filename_case_insensitive(struct torture_context *torture, struct smbcli_state *cli)
{
       TALLOC_CTX *mem_ctx;
       const char *dirname = "insensitive";
       const char *ucase_dirname = "InSeNsItIvE";
       char *fname = "foo~!@#$%^&()-=_+{}[]`";
       char *fname_upper = "FOO~!@#$%^&()-=_+{}[]`";
       char chinese[256] = {'\0'};
       char *fpath;
       char *fpath_dst;
       int fnum;
       int counter = 0;
       bool ret = false;
       NTSTATUS status;

       if (!(mem_ctx = talloc_init("test_mix_filename_case_insensitive"))) {
              torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
              return false;
       }  
       torture_assert(torture, torture_setup_dir(cli, dirname), "creating test directory");
       gen_chinese_string(1, chinese);
       if(!(fpath = talloc_asprintf(mem_ctx, "%s\\%s%s", dirname, fname, chinese))){
               ret = false;
               goto done;
       }

       torture_comment(torture, "creating file.\n");
        fnum = smbcli_open(cli->tree, fpath, O_RDWR | O_CREAT, DENY_NONE);
       if (fnum == -1) {
              torture_result(torture, TORTURE_FAIL,
                     "Could not create file %s: %s", fpath,
                      smbcli_errstr(cli->tree));
              goto done;
       }
       smbcli_close(cli->tree, fnum);

       torture_comment(torture, "listing file.\n");
       smbcli_list(cli->tree, talloc_asprintf(
                         mem_ctx, "%s\\*", ucase_dirname),
                  FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN
                  |FILE_ATTRIBUTE_SYSTEM,
                  count_fn, (void *)&counter);

       if (counter == 3) {
              ret = true;
       }
       else {
              torture_result(torture, TORTURE_FAIL,
                     "expected 3 entries, got %d", counter);
              ret = false;
              goto done;
       }

       torture_comment(torture, "unlinking file.\n");
       if (!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s%s", ucase_dirname, fname_upper, chinese))){
                ret = false;
                goto done;
       }
       status = smbcli_unlink(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
           goto done;  

       torture_comment(torture, "creating directory.\n");
       status = smbcli_mkdir(cli->tree, fpath);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
                goto done;

  
        torture_comment(torture, "check directory.\n");
        status = smbcli_chkpath(cli->tree, fpath_dst);
        CHECK_STATUS(torture, status, NT_STATUS_OK);
        if(false == ret)
               goto done;

     
       torture_comment(torture, "removing directory.\n");
       status = smbcli_rmdir(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status,  NT_STATUS_OK);
 
  done:
       talloc_free(mem_ctx);
       return ret;
}

/*
max filename length test
1. create a file with max length filename
2. list the file
3. unlink the file
4. create a dir with max length filename
5. check the dir path
6. remove the dir
*/
bool test_max_filename_case_insensitive(struct torture_context *torture, struct smbcli_state *cli)
{
       TALLOC_CTX *mem_ctx;
       const char *dirname = "insensitive";
       const char *ucase_dirname = "InSeNsItIvE";
       char fname[256] = {'\0'};
       char fname_upper[256] = {'\0'};
       char *fpath;
       char *fpath_dst;
       int fnum;
       int counter = 0;
       int filename_length = 228;
       bool ret = false;
       NTSTATUS status;

       if (!(mem_ctx = talloc_init("test_max_filename_case_insensitive"))) {
              torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
              return false;
       }
       torture_assert(torture, torture_setup_dir(cli, dirname), "creating test directory");
       gen_length_string(filename_length, false, fname);
      
       if(!(fpath = talloc_asprintf(mem_ctx, "%s\\%s", dirname, fname))){
               ret = false;
               goto done;
       }
      
       torture_comment(torture, "creating file.\n");
       fnum = smbcli_open(cli->tree, fpath, O_RDWR | O_CREAT, DENY_NONE);
       if (fnum == -1) {
              torture_result(torture, TORTURE_FAIL,
                     "Could not create file %s: %s", fpath,
                      smbcli_errstr(cli->tree));
              goto done;
       }
       smbcli_close(cli->tree, fnum);

      
       torture_comment(torture, "listing file.\n");
       smbcli_list(cli->tree, talloc_asprintf(
                         mem_ctx, "%s\\*", ucase_dirname),
                  FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN
                  |FILE_ATTRIBUTE_SYSTEM,
                  count_fn, (void *)&counter);

       if (counter == 3) {
              ret = true;
       }
       else {
              torture_result(torture, TORTURE_FAIL,
                     "expected 3 entries, got %d", counter);
              ret = false;
              goto done;
       }
      
       torture_comment(torture, "unlinking file.\n");
       gen_length_string(filename_length, true, fname_upper);
       if (!(fpath_dst = talloc_asprintf(mem_ctx, "%s\\%s", ucase_dirname, fname_upper))){
                ret = false;
                goto done;
       }
       status = smbcli_unlink(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
           goto done;


       
        torture_comment(torture, "creating directory.\n");
        status = smbcli_mkdir(cli->tree, fpath);
        CHECK_STATUS(torture, status, NT_STATUS_OK);
        if(false == ret)
                goto done;

        
       torture_comment(torture, "chking directory.\n"); 
       status = smbcli_chkpath(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret)
                goto done;

    
       torture_comment(torture, "remove directory.\n");
       status = smbcli_rmdir(cli->tree, fpath_dst);
       CHECK_STATUS(torture, status,  NT_STATUS_OK);
        
 done:
       talloc_free(mem_ctx);
       return ret;
}

/*
dir create and tranverse test
1. create the dir with sub-dir and files
2. tranverse the dir
3. remove dir
*/
bool test_largedir_case_insensitive(struct torture_context *torture, struct smbcli_state *cli)
{
       const char *dirname = "insensitive";
       const char *ucase_dirname = "InSeNsItIvE";
       int ret = true;
       int level = 10;
       int level_cnt=1;

       torture_assert(torture, torture_setup_dir(cli, dirname), "creating test directory");
      
     
       torture_comment(torture, "creating large directory.\n");
       ret = create_largedir_case_insensitive( torture,  cli,  ucase_dirname, level, level_cnt);
       if(false == ret){
              torture_result(torture, TORTURE_FAIL, "create large directory failed.\n");
              return ret;
       }

       
       torture_comment(torture, "tranversing large directory.\n");
       ret = tranverse_largedir_case_insensitive(torture, cli, dirname);
       if(false == ret)
       {
              torture_result(torture, TORTURE_FAIL, "tranverse large directory failed.\n");
              return ret;
       }

       
       torture_comment(torture, "removing directory.\n");
       if(smbcli_deltree(cli->tree, dirname) == -1){
              torture_result(torture, TORTURE_FAIL, "remove large directory failed.\n");
              return false;
       }

       return ret;
}
/*
case sensitive and case insensitive mix test
1. set the connection in case sensitive mode
2. create a file in lower format
3. create a file in upper format
4. set the connection in case insensitive mode
5. write into the first file
6. get the size of file in lower format filename
7. get the size of file in upper format filename
*/
bool test_mix_protocol_access(struct torture_context *torture, struct smbcli_state *cli)
{
       TALLOC_CTX *mem_ctx;
       const char *dirname = "insensitive";
       const char *ucase_dirname = "InSeNsItIvE";
       const char *fname = "foo";
       const char *fname_upper = "FOO";
       char *fpath = NULL;
       char *fpath_upper = NULL;
       int fnum = -1;
       int fnum_upper = -1;
       int counter = 0;
       bool ret = true;
       union smb_open io;
       NTSTATUS status;
       union smb_write fio;
       union smb_close close_io;
       uint8_t *buf;
       const int maxsize = 1024;
       size_t file_size = 0;
       size_t file_size2 = 0;
      
       set_case_sensitive(cli, true);

       if (!(mem_ctx = talloc_init("test_mix_protocol_access"))) {
              torture_result(torture, TORTURE_FAIL, "talloc_init failed\n");
              return false;
       }
       torture_assert(torture, torture_setup_dir(cli, dirname), "creating test directory");
       if (!(fpath = talloc_asprintf(mem_ctx, "%s\\%s", dirname, fname))) {
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed.\n");
              ret = false;
              goto done;
       }
       if (!(fpath_upper = talloc_asprintf(mem_ctx, "%s\\%s", dirname, fname_upper))) {
              torture_result(torture, TORTURE_FAIL, "talloc_asprintf failed.\n");
              ret = false;
              goto done;
       }
       torture_comment(torture, "creating file in lowner format.\n");
       fnum = smbcli_open(cli->tree, fpath, O_RDWR | O_CREAT, DENY_NONE);
       if (fnum == -1) {
              torture_result(torture, TORTURE_FAIL,
                     "Could not create file %s: %s", fpath,
                      smbcli_errstr(cli->tree));
              ret = false;
              goto done;
       }

       torture_comment(torture, "creating file in upper format.\n");
       io.generic.level = RAW_OPEN_NTCREATEX;
       io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
       io.ntcreatex.in.root_fid.fnum = 0;
       io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
       io.ntcreatex.in.alloc_size = 1024*1024;
       io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
       io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
       io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
       io.ntcreatex.in.create_options = 0;
       io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
       io.ntcreatex.in.security_flags = 0;
       io.ntcreatex.in.fname = fpath_upper;
       status = smb_raw_open(cli->tree, torture, &io);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       if(false == ret){
              goto done;
       }

       set_case_sensitive(cli, false);
       torture_comment(torture, "writing into  file in lower format.\n");
       buf = talloc_zero_array(mem_ctx, uint8_t, maxsize);
       fio.generic.level = RAW_WRITE_WRITE;
       fio.write.in.file.fnum = fnum;
       fio.write.in.count = 100;
       fio.write.in.offset = 0;
       fio.write.in.remaining = 0;
       fio.write.in.data = buf;
       status = smb_raw_write(cli->tree, &fio);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       CHECK_VALUE(fio.write.out.nwritten, fio.write.in.count);
       smbcli_close(cli->tree, fnum);

       status = smbcli_getatr(cli->tree, fpath_upper, NULL, &file_size, NULL);
       CHECK_STATUS(torture, status, NT_STATUS_OK);

       status = smbcli_getatr(cli->tree, fpath, NULL, &file_size2, NULL);
       CHECK_STATUS(torture, status, NT_STATUS_OK);

       ret = (file_size == file_size2)?true:false;
       if (ret == false){
              goto done;
       }
       close_io.close.level = RAW_CLOSE_CLOSE;
       close_io.close.in.file.fnum = io.ntcreatex.out.file.fnum;
       close_io.close.in.write_time = 0;
       status = smb_raw_close(cli->tree, &close_io);
       CHECK_STATUS(torture, status, NT_STATUS_OK);

       torture_comment(torture, "unlink one file in lower format.\n");
       status = smbcli_unlink(cli->tree, fpath);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       status = smbcli_getatr(cli->tree, fpath, NULL, &file_size2, NULL);
       CHECK_STATUS(torture, status, NT_STATUS_OK);
       ret = (file_size2 != file_size)?true:false;
       if (ret == false){
              goto done;
       }

       if(smbcli_deltree(cli->tree, dirname) == -1){
              torture_result(torture, TORTURE_FAIL, "remove directory failed.\n");
              ret = false;
              goto done;
       } 
done:
       talloc_free(mem_ctx);
       return ret;
}


struct torture_suite *torture_raw_case_insensitive(TALLOC_CTX *mem_ctx)
{
       struct torture_suite *suite = torture_suite_create(mem_ctx, "samba3caseinsensitive");

       torture_suite_add_1smb_test(suite, "normal file operation", test_file_case_insensitive);
       torture_suite_add_1smb_test(suite, "normal dir operation", test_dir_case_insensitive);
       torture_suite_add_1smb_test(suite, "file and dir have same name", test_file_dir_case_insensitive);
       torture_suite_add_1smb_test(suite, "special character filename test", test_special_character_filename_case_insensitive);
       torture_suite_add_1smb_test(suite, "chinese filename test",  test_chinese_case_insensitive);
       torture_suite_add_1smb_test(suite, "mix filename test", test_mix_filename_case_insensitive);
       torture_suite_add_1smb_test(suite, "max length filename test", test_max_filename_case_insensitive);
       torture_suite_add_1smb_test(suite, "large directory test",  test_largedir_case_insensitive);
       torture_suite_add_1smb_test(suite, "test mix protocol access", test_mix_protocol_access);
       return suite;
}


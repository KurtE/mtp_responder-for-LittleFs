// Storage.cpp - Teensy MTP Responder library
// Copyright (C) 2017 Fredrik Hubinette <hubbe@hubbe.net>
//
// With updates from MichaelMC and Yoong Hor Meng <yoonghm@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// modified for SDFS by WMXZ
// Nov 2020 adapted to SdFat-beta / SD combo

#include "core_pins.h"
#include "usb_dev.h"
#include "usb_serial.h"

#include "Storage_RAM.h"

  extern LittleFS_RAM sdp[];
  #define sdp_begin(x,y) sdp[x].begin(y)
  #define sdp_open(x,y,z) sdp[x].open(y,z)
  #define sdp_mkdir(x,y) sdp[x].mkdir(y)
  #define sdp_rename(x,y,z) sdp[x].rename(y,z)
  #define sdp_remove(x,y) sdp[x].remove(y)
  #define sdp_rmdir(x,y) sdp[x].rmdir(y)

  #define sdp_isOpen(x)  (x)
  #define sdp_getName(x,y,n) strcpy(y,x.name())

  #define indexFile "/mtpindex.dat"

  
// TODO:
//   support multiple storages
//   support serialflash
//   partial object fetch/receive
//   events (notify usb host when local storage changes) (But, this seems too difficult)

// These should probably be weak.
void mtp_yield_ram() {}
void mtp_lock_storage_ram(bool lock) {}

  bool MTPStorage_RAM::readonly(uint32_t storage) { return false; }
  bool MTPStorage_RAM::has_directories(uint32_t storage) { return true; }


//  uint64_t MTPStorage_RAM::size() { return (uint64_t)512 * (uint64_t)spf.clusterCount()     * (uint64_t)spf.sectorsPerCluster(); }
//  uint64_t MTPStorage_RAM::free() { return (uint64_t)512 * (uint64_t)spf.freeClusterCount() * (uint64_t)spf.sectorsPerCluster(); }
  uint32_t MTPStorage_RAM::clusterCount(uint32_t storage) { return  sdp[storage-1].totalSize(); }
  uint32_t MTPStorage_RAM::freeClusters(uint32_t storage) { return (sdp[storage-1].totalSize()-sdp[storage-1].usedSize()); }
  uint32_t MTPStorage_RAM::clusterSize(uint32_t storage) { return 0; }  

  void MTPStorage_RAM::CloseIndex()
  {
    mtp_lock_storage_ram(true);
    if(sdp_isOpen(index_)) index_.close();
    mtp_lock_storage_ram(false);
    index_generated = false;
    index_entries_ = 0;
  }

  void MTPStorage_RAM::OpenIndex() 
  { if(sdp_isOpen(index_)) return; // only once
    mtp_lock_storage_ram(true);
    index_=sdp_open(0,indexFile, FILE_WRITE);
    mtp_lock_storage_ram(false);
  }

  void MTPStorage_RAM::ResetIndex() {
    if(!sdp_isOpen(index_)) return;
    
    CloseIndex();
    OpenIndex();

    all_scanned_ = false;
    open_file_ = 0xFFFFFFFEUL;
  }

  void MTPStorage_RAM::WriteIndexRecord(uint32_t i, const Record& r) 
  {
    OpenIndex();
    mtp_lock_storage_ram(true);
    index_.seek(sizeof(r) * i);
    index_.write((char*)&r, sizeof(r));
    mtp_lock_storage_ram(false);
  }

  uint32_t MTPStorage_RAM::AppendIndexRecord(const Record& r) 
  { uint32_t new_record = index_entries_++;
    WriteIndexRecord(new_record, r);
    return new_record;
  }

  // TODO(hubbe): Cache a few records for speed.
  Record MTPStorage_RAM::ReadIndexRecord(uint32_t i) 
  {
    Record ret;
    memset(&ret, 0, sizeof(ret));
    if (i > index_entries_) 
    { memset(&ret, 0, sizeof(ret));
      return ret;
    }
    OpenIndex();
    mtp_lock_storage_ram(true);
    index_.seek(sizeof(ret) * i);
    index_.read((char *)&ret, sizeof(ret));
    mtp_lock_storage_ram(false);
    return ret;
  }

  uint16_t MTPStorage_RAM::ConstructFilename(int i, char* out, int len) // construct filename rexursively
  {
    Record tmp = ReadIndexRecord(i);
      
    if (tmp.parent==(unsigned)i) 
    { strcpy(out, "/");
      return tmp.store;
    }
    else 
    { ConstructFilename(tmp.parent, out, len);
      if (out[strlen(out)-1] != '/') strcat(out, "/");
      if(((strlen(out)+strlen(tmp.name)+1) < (unsigned) len)) strcat(out, tmp.name);
      return tmp.store;
    }
  }

  void MTPStorage_RAM::OpenFileByIndex(uint32_t i, uint32_t mode) 
  {
    if (open_file_ == i && mode_ == mode) return;
    char filename[256];
    uint16_t store = ConstructFilename(i, filename, 256);
    mtp_lock_storage_ram(true);
    if(sdp_isOpen(file_)) file_.close();
    file_=sdp_open(store,filename,mode);
    open_file_ = i;
    mode_ = mode;
    mtp_lock_storage_ram(false);
  }

  // MTP object handles should not change or be re-used during a session.
  // This would be easy if we could just have a list of all files in memory.
  // Since our RAM is limited, we'll keep the index in a file instead.
  void MTPStorage_RAM::GenerateIndex(uint32_t storage)
  { if (index_generated) return; 
    index_generated = true;
    // first remove old index file
    mtp_lock_storage_ram(true);
    sdp_remove(0,indexFile);
    mtp_lock_storage_ram(false);

    index_entries_ = 0;
    Record r;
    for(int ii=0; ii<num_storage; ii++)
    {
      r.store = ii; // store is typically (storage-1) //store 0...6; storage 1...7
      r.parent = ii;
      r.sibling = 0;
      r.child = 0;
      r.isdir = true;
      r.scanned = false;
      strcpy(r.name, "/");
      AppendIndexRecord(r);
    }
  }

  void MTPStorage_RAM::ScanDir(uint32_t storage, uint32_t i) 
  { Record record = ReadIndexRecord(i);
    if (record.isdir && !record.scanned) {
      OpenFileByIndex(i);
      if (!sdp_isOpen(file_)) return;
    
      int sibling = 0;
      while (true) 
      { mtp_lock_storage_ram(true);
        child_=file_.openNextFile();
        mtp_lock_storage_ram(false);
        if(!sdp_isOpen(child_)) break;

        Record r;
        r.store = record.store;
        r.parent = i;
        r.sibling = sibling;
        r.isdir = child_.isDirectory();
        r.child = r.isdir ? 0 : child_.size();
        r.scanned = false;
        sdp_getName(child_,r.name,64);
        sibling = AppendIndexRecord(r);
        child_.close();
      }
      record.scanned = true;
      record.child = sibling;
      WriteIndexRecord(i, record);
    }
  }

  void MTPStorage_RAM::ScanAll(uint32_t storage) 
  { if (all_scanned_) return;
    all_scanned_ = true;

    GenerateIndex(storage);
    for (uint32_t i = 0; i < index_entries_; i++)  ScanDir(storage,i);
  }

  void  MTPStorage_RAM::setStorageNumbers(const char **str, int num) {sd_str = str; num_storage=num;}
  uint32_t MTPStorage_RAM::getNumStorage() {return num_storage;}
  const char * MTPStorage_RAM::getStorageName(uint32_t storage) {return sd_str[storage-1];}

  void MTPStorage_RAM::StartGetObjectHandles(uint32_t storage, uint32_t parent) 
  { 
    GenerateIndex(storage);
    if (parent) 
    { if (parent == 0xFFFFFFFF) parent = storage-1; // As per initizalization

      ScanDir(storage, parent);
      follow_sibling_ = true;
      // Root folder?
      next_ = ReadIndexRecord(parent).child;
    } 
    else 
    { 
      ScanAll(storage);
      follow_sibling_ = false;
      next_ = 1;
    }
  }

  uint32_t MTPStorage_RAM::GetNextObjectHandle(uint32_t  storage)
  {
    while (true) 
    { if (next_ == 0) return 0;

      int ret = next_;
      Record r = ReadIndexRecord(ret);
      if (follow_sibling_) 
      { next_ = r.sibling;
      } 
      else 
      { next_++;
        if (next_ >= index_entries_) next_ = 0;
      }
      if (r.name[0]) return ret;
    }
  }

  void MTPStorage_RAM::GetObjectInfo(uint32_t handle, char* name, uint32_t* size, uint32_t* parent, uint16_t *store)
  {
    Record r = ReadIndexRecord(handle);
    strcpy(name, r.name);
    *parent = r.parent;
    *size = r.isdir ? 0xFFFFFFFFUL : r.child;
    *store = r.store;
  }

  uint32_t MTPStorage_RAM::GetSize(uint32_t handle) 
  {
    return ReadIndexRecord(handle).child;
  }

  void MTPStorage_RAM::read(uint32_t handle, uint32_t pos, char* out, uint32_t bytes)
  {
    OpenFileByIndex(handle);
    mtp_lock_storage_ram(true);
    file_.seek(pos);
    file_.read(out,bytes);
    mtp_lock_storage_ram(false);
  }

  bool MTPStorage_RAM::DeleteObject(uint32_t object)
  {
    char filename[256];
    Record r;
    while (true) {
      r = ReadIndexRecord(object == 0xFFFFFFFFUL ? 0 : object);
      if (!r.isdir) break;
      if (!r.child) break;
      if (!DeleteObject(r.child))  return false;
    }

    // We can't actually delete the root folder,
    // but if we deleted everything else, return true.
    if (object == 0xFFFFFFFFUL) return true;

    ConstructFilename(object, filename, 256);
    bool success;
    mtp_lock_storage_ram(true);
    if (r.isdir) success = sdp_rmdir(0,filename); else  success = sdp_remove(0,filename);
    mtp_lock_storage_ram(false);
    if (!success) return false;
    
    r.name[0] = 0;
    int p = r.parent;
    WriteIndexRecord(object, r);
    Record tmp = ReadIndexRecord(p);
    if (tmp.child == object) 
    { tmp.child = r.sibling;
      WriteIndexRecord(p, tmp);
    } 
    else 
    { int c = tmp.child;
      while (c) 
      { tmp = ReadIndexRecord(c);
        if (tmp.sibling == object) 
        { tmp.sibling = r.sibling;
          WriteIndexRecord(c, tmp);
          break;
        } 
        else 
        { c = tmp.sibling;
        }
      }
    }
    return true;
  }

  uint32_t MTPStorage_RAM::Create(uint32_t storage, uint32_t parent,  bool folder, const char* filename)
  {
    uint32_t ret;
    if (parent == 0xFFFFFFFFUL) parent = 0;
    Record p = ReadIndexRecord(parent);
    Record r;
    if (strlen(filename) > 62) return 0;
    strcpy(r.name, filename);
    r.store = p.store;
    r.parent = parent;
    r.child = 0;
    r.sibling = p.child;
    r.isdir = folder;
    // New folder is empty, scanned = true.
    r.scanned = 1;
    ret = p.child = AppendIndexRecord(r);
    WriteIndexRecord(parent, p);
    if (folder) 
    {
      char filename[256];
      uint16_t store =ConstructFilename(ret, filename, 256);
      mtp_lock_storage_ram(true);
      sdp_mkdir(store,filename);
      mtp_lock_storage_ram(false);
    } 
    else 
    {
      OpenFileByIndex(ret, FILE_WRITE);
    }
    return ret;
  }

  void MTPStorage_RAM::write(const char* data, uint32_t bytes)
  {
      mtp_lock_storage_ram(true);
      file_.write(data,bytes);
      mtp_lock_storage_ram(false);
  }

  void MTPStorage_RAM::close() 
  {
    mtp_lock_storage_ram(true);
    uint64_t size = file_.size();
    file_.close();
    mtp_lock_storage_ram(false);
    Record r = ReadIndexRecord(open_file_);
    r.child = size;
    WriteIndexRecord(open_file_, r);
    open_file_ = 0xFFFFFFFEUL;
  }

  bool MTPStorage_RAM::rename(uint32_t handle, const char* name) 
  { char oldName[256];
    char newName[256];
    char temp[64];

    uint16_t store = ConstructFilename(handle, oldName, 256);
    Serial.println(oldName);

    Record p1 = ReadIndexRecord(handle);
    strcpy(temp,p1.name);
    strcpy(p1.name,name);

    WriteIndexRecord(handle, p1);
    ConstructFilename(handle, newName, 256);

    if (sdp_rename(store,oldName,newName)) return true;

    // rename failed; undo index update
    strcpy(p1.name,temp);
    WriteIndexRecord(handle, p1);
    return false;
  }

  void MTPStorage_RAM::printIndexList(void)
  {
    for(uint32_t ii=0; ii<index_entries_; ii++)
    { Record p = ReadIndexRecord(ii);
      Serial.printf("%d: %d %d %d %d %s\n",ii, p.isdir,p.parent,p.sibling,p.child,p.name);
    }
  }

  void MTPStorage_RAM::printRecord(int h, Record *p) 
  { Serial.printf("%d: %d %d %d %d\n",h, p->isdir,p->parent,p->sibling,p->child); }
  
/*
 * //index list management for moving object around
 * p1 is record of handle
 * p2 is record of new dir
 * p3 is record of old dir
 * 
 *  // remove from old direcory
 * if p3.child == handle  / handle is last in old dir
 *      p3.child = p1.sibling   / simply relink old dir
 *      save p3
 * else
 *      px record of p3.child
 *      while( px.sibling != handle ) update px = record of px.sibling
 *      px.sibling = p1.sibling
 *      save px
 * 
 *  // add to new directory
 * p1.parent = new
 * p1.sibling = p2.child
 * p2.child = handle
 * save p1
 * save p2
 * 
 */


  bool MTPStorage_RAM::move(uint32_t handle, uint32_t newParent ) 
  { 
    Record p1 = ReadIndexRecord(handle); 

    uint32_t oldParent = p1.parent;
    Record p2 = ReadIndexRecord(newParent);
    Record p3 = ReadIndexRecord(oldParent); 

    char oldName[256];
    uint16_t store0 = ConstructFilename(handle, oldName, 256);

    if(p1.store != p2.store) return false;

    // remove from old direcory
    if(p3.child==handle)
    {
      p3.child = p1.sibling;
      WriteIndexRecord(oldParent, p3);    
    }
    else
    { uint32_t jx = p3.child;
      Record px = ReadIndexRecord(jx); 
      while(handle != px.sibling)
      {
        jx = px.sibling;
        px = ReadIndexRecord(jx); 
      }
      px.sibling = p1.sibling;
      WriteIndexRecord(jx, px);
    }
  
    // add to new directory
    p1.parent = newParent;
    p1.sibling = p2.child;
    p2.child = handle;
    WriteIndexRecord(handle, p1);
    WriteIndexRecord(newParent,p2);

    char newName[256];
    ConstructFilename(handle, newName, 256);
//    Serial.println(newName);
//    printIndexList();

    return sdp_rename(store0,oldName,newName);
  }
  
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

#include "core_pins.h"
#include "usb_dev.h"
#include "usb_serial.h"

  #include "Storage_SPI.h"
  
  // Call-back for file timestamps.  Only called for file create and sync().
  #include "TimeLib.h"
  // Call back for file timestamps.  Only called for file create and sync().


LittleFS_SPIFlash spf;

  
 bool Storage_init_spi(uint8_t cspin, SPIClass &spiport)
{
	
	pinMode(cspin, INPUT_PULLUP);
	if (!spf.begin(cspin, spiport)) {
		return false;
		//spf.errorHalt("spf.begin failed");
	} else {
		delay(10);
		return true;
	}
    // Set Time callback
    //FsDateTime::callback = dateTime;
  }


// TODO:
//   support multiple storages
//   support serialflash
//   partial object fetch/receive
//   events (notify usb host when local storage changes)

// These should probably be weak.
void mtp_yield_spi() {}
void mtp_lock_storage_spi(bool lock) {}

  bool MTPStorage_SPI::readonly() { return false; }
  bool MTPStorage_SPI::has_directories() { return true; }
  
  void MTPStorage_SPI::capacity(){
	mem_available = spf.totalSize();;
	mem_used = spf.usedSize();
	mem_free = mem_available - mem_used;
  }
  

//  uint64_t MTPStorage_SPI::size() { return (uint64_t)512 * (uint64_t)spf.clusterCount()     * (uint64_t)spf.sectorsPerCluster(); }
//  uint64_t MTPStorage_SPI::free() { return (uint64_t)512 * (uint64_t)spf.freeClusterCount() * (uint64_t)spf.sectorsPerCluster(); }
  uint32_t MTPStorage_SPI::clusterCount() { capacity(); return mem_available; }
  uint32_t MTPStorage_SPI::freeClusters() { capacity(); return mem_free; }
  uint32_t MTPStorage_SPI::clusterSize() { return 0; }  


  void MTPStorage_SPI::ResetIndex() {
    if(!index_) return;
    
    mtp_lock_storage_spi(true);
    if(index_.available()) index_.close();
    spf.remove(indexFile);
    index_ = spf.open(indexFile, FILE_READ);
    mtp_lock_storage_spi(false);

    all_scanned_ = false;
    index_generated=false;
    open_file_ = 0xFFFFFFFEUL;
  }

  void MTPStorage_SPI::CloseIndex()
  {
    mtp_lock_storage_spi(true);
    index_.close();
    mtp_lock_storage_spi(false);
    index_generated = false;
    index_entries_ = 0;
  }

  void MTPStorage_SPI::OpenIndex() 
  { 
  if(index_) {
	  return; // only once
  }
    mtp_lock_storage_spi(true);
    index_ = spf.open(indexFile, FILE_WRITE);
    mtp_lock_storage_spi(false);
  }

  void MTPStorage_SPI::WriteIndexRecord(uint32_t i, const Record& r) 
  {
    OpenIndex();
    mtp_lock_storage_spi(true);
	Serial.println(index_);
    index_.seek(sizeof(r) * i);
    index_.write((char*)&r, sizeof(r));
    mtp_lock_storage_spi(false);
  }

  uint32_t MTPStorage_SPI::AppendIndexRecord(const Record& r) 
  {
    uint32_t new_record = index_entries_++;
    WriteIndexRecord(new_record, r);
    return new_record;
  }

  // TODO(hubbe): Cache a few records for speed.
  Record MTPStorage_SPI::ReadIndexRecord(uint32_t i) 
  {
    Record ret;
    if (i > index_entries_) 
    { memset(&ret, 0, sizeof(ret));
      return ret;
    }
    OpenIndex();
    mtp_lock_storage_spi(true);
    index_.seek(sizeof(ret) * i);
    index_.read((char *)&ret, sizeof(ret));
    mtp_lock_storage_spi(false);
    return ret;
  }

  void MTPStorage_SPI::ConstructFilename(int i, char* out, int len) // construct filename rexursively
  {
    if (i == 0) 
    { strcpy(out, "/");
    }
    else 
    { Record tmp = ReadIndexRecord(i);
      ConstructFilename(tmp.parent, out, len);
      if (out[strlen(out)-1] != '/') strcat(out, "/");
      if(((strlen(out)+strlen(tmp.name)+1) < (unsigned) len)) strcat(out, tmp.name);
    }
  }

  void MTPStorage_SPI::OpenFileByIndex(uint32_t i, uint32_t mode) 
  {
    if (open_file_ == i && mode_ == mode) return;
    char filename[256];
    ConstructFilename(i, filename, 256);
    mtp_lock_storage_spi(true);
    if(file_) file_.close();
    file_ = spf.open(filename,mode);
    open_file_ = i;
    mode_ = mode;
    mtp_lock_storage_spi(false);
  }

  // MTP object handles should not change or be re-used during a session.
  // This would be easy if we could just have a list of all files in memory.
  // Since our RAM is limited, we'll keep the index in a file instead.
  void MTPStorage_SPI::GenerateIndex()
  {
	///Serial.println("GenerateIndex");
    if (index_generated) return;
    index_generated = true;

    // first remove old index file
    mtp_lock_storage_spi(true);
    spf.remove(indexFile);
    mtp_lock_storage_spi(false);
    index_entries_ = 0;

    Record r;
    r.parent = 0;
    r.sibling = 0;
    r.child = 0;
    r.isdir = true;
    r.scanned = false;
    strcpy(r.name, "/");
    AppendIndexRecord(r);
  }


  void MTPStorage_SPI::ScanDir(uint32_t i) 
  {
    Record record = ReadIndexRecord(i);
    if (record.isdir && !record.scanned) {
		//need to convert record name to string and add a "/" if not just a /
	//Serial.println(record.name);
	if(strcmp(record.name, "/") != 0) {
		char str1[65] = "/";
		strncat(str1, record.name,64);
		//Serial.println(str1);
		printDirectory1(spf.open(str1), 0);
	} else {
		printDirectory1(spf.open(record.name), 0);
	}

      OpenFileByIndex(i);
	   mtp_lock_storage_spi(true);
        mtp_lock_storage_spi(false);
      if (!file_) return;
      int sibling = 0;
	  
      for(uint16_t rec_count=0; rec_count<entry_cnt; rec_count++) 
      {
        Record r;
        r.parent = i;
        r.sibling = sibling;
        r.isdir = entries[rec_count].isDir;
        r.child = r.isdir ? 0 : entries[rec_count].size;
        r.scanned = false;
		memset(r.name, 0, sizeof(r.name));
		for(uint8_t j=0;j<entries[rec_count].fnamelen;j++)
				r.name[j] = entries[rec_count].name[j];
        sibling = AppendIndexRecord(r);

Serial.printf("ScanDir1\n\tIndex: %d\n", i);
Serial.printf("\tname: %s, parent: %d, child: %d, sibling: %d\n", r.name, r.parent, r.child, r.sibling);
Serial.printf("\tIsdir: %d, IsScanned: %d\n", r.isdir, r.scanned);
      }
      record.scanned = true;
      record.child = sibling;
      WriteIndexRecord(i, record);
    }
  }

  void MTPStorage_SPI::ScanAll() 
  {
    if (all_scanned_) return;
    all_scanned_ = true;

    GenerateIndex();
    for (uint32_t i = 0; i < index_entries_; i++)  ScanDir(i);
  }

  void MTPStorage_SPI::StartGetObjectHandles(uint32_t parent) 
  {
    GenerateIndex();
    if (parent) 
    { if (parent == 0xFFFFFFFF) parent = 0;

      ScanDir(parent);
      follow_sibling_ = true;
      // Root folder?
      next_ = ReadIndexRecord(parent).child;
    } 
    else 
    { ScanAll();
      follow_sibling_ = false;
      next_ = 1;
    }
  }

  uint32_t MTPStorage_SPI::GetNextObjectHandle()
  {
    while (true) {
      if (next_ == 0) return 0;

      int ret = next_;
      Record r = ReadIndexRecord(ret);
      if (follow_sibling_) 
      { next_ = r.sibling;
      } 
      else 
      {
        next_++;
        if (next_ >= index_entries_) next_ = 0;
      }
      if (r.name[0]) return ret;
    }
  }

  void MTPStorage_SPI::GetObjectInfo(uint32_t handle, char* name, uint32_t* size, uint32_t* parent)
  {
    Record r = ReadIndexRecord(handle);
    strcpy(name, r.name);
    *parent = r.parent;
    *size = r.isdir ? 0xFFFFFFFFUL : r.child;
  }

  uint32_t MTPStorage_SPI::GetSize(uint32_t handle) 
  {
    return ReadIndexRecord(handle).child;
  }

  void MTPStorage_SPI::read(uint32_t handle, uint32_t pos, char* out, uint32_t bytes)
  {
    OpenFileByIndex(handle);
    mtp_lock_storage_spi(true);
    file_.seek(pos);
    file_.read(out,bytes);
    mtp_lock_storage_spi(false);
  }

  bool MTPStorage_SPI::DeleteObject(uint32_t object)
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
    mtp_lock_storage_spi(true);
    if (r.isdir) success = spf.rmdir(filename); else  success = spf.remove(filename);
    mtp_lock_storage_spi(false);
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

  uint32_t MTPStorage_SPI::Create(uint32_t parent,  bool folder, const char* filename)
  {
    uint32_t ret;
    if (parent == 0xFFFFFFFFUL) parent = 0;
    Record p = ReadIndexRecord(parent);
    Record r;
    if (strlen(filename) > 62) return 0;
    strcpy(r.name, filename);
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
      ConstructFilename(ret, filename, 256);
      mtp_lock_storage_spi(true);
      spf.mkdir(filename);
      mtp_lock_storage_spi(false);
    } 
    else 
    {
      OpenFileByIndex(ret, FILE_WRITE);
    }
    return ret;
  }

  void MTPStorage_SPI::write(const char* data, uint32_t bytes)
  {
      mtp_lock_storage_spi(true);
      file_.write(data,bytes);
      mtp_lock_storage_spi(false);
  }

  void MTPStorage_SPI::close() 
  {
    mtp_lock_storage_spi(true);
    uint64_t size = file_.size();
    file_.close();
    mtp_lock_storage_spi(false);
    Record r = ReadIndexRecord(open_file_);
    r.child = size;
    WriteIndexRecord(open_file_, r);
    open_file_ = 0xFFFFFFFEUL;
  }

  void MTPStorage_SPI::rename(uint32_t handle, const char* name) 
  { 
    char oldName[256];
    char newName[256];
	
Serial.printf("-----------------  Rename Debug ---------------\n");

    ConstructFilename(handle, oldName, 256);
Serial.printf("Handle/oldname:  %d, %s\n", handle, oldName);
    Record p1 = ReadIndexRecord(handle);
    strcpy(p1.name,name);
	
Serial.printf("Rename record before\n\tIndex: %d\n", handle);
Serial.printf("\tname: %s, parent: %d, child: %d, sibling: %d\n", p1.name, p1.parent, p1.child, p1.sibling);
Serial.printf("\tIsdir: %d, IsScanned: %d\n", p1.isdir, p1.scanned);
	
    WriteIndexRecord(handle, p1);
	
p1 = ReadIndexRecord(handle);
Serial.printf("Rename record After\n\tIndex: %d\n", handle);
Serial.printf("\tname: %s, parent: %d, child: %d, sibling: %d\n", p1.name, p1.parent, p1.child, p1.sibling);
Serial.printf("\tIsdir: %d, IsScanned: %d\n", p1.isdir, p1.scanned);

    ConstructFilename(handle, newName, 256);
Serial.printf("Handle/oldname:  %d, %s\n", handle, newName);
    spf.rename(oldName,newName);

  }

  void MTPStorage_SPI::move(uint32_t handle, uint32_t newParent ) 
  { 
    char oldName[256];
    char newName[256];

    ConstructFilename(handle, oldName, 256);
    Record p1 = ReadIndexRecord(handle);

    if (newParent == 0xFFFFFFFFUL) newParent = 0;
    Record p2 = ReadIndexRecord(newParent); // is pointing to last object in directory

    p1.sibling = p2.child;
    p1.parent = newParent;

    p2.child = handle; 
    WriteIndexRecord(handle, p1);
    WriteIndexRecord(newParent, p2);

    ConstructFilename(handle, newName, 256);
    spf.rename(oldName,newName);
  }
  

void MTPStorage_SPI::printDirectory() {
  Serial.println("printDirectory\n--------------");
  printDirectory1(spf.open("/"), 0);
  //Serial.println();
}


void MTPStorage_SPI::printDirectory1(File dir, int numTabs) {
  //dir.whoami();
  entry_cnt = 0;
  while (true) {
    entry_cnt = entry_cnt + 1;
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      //Serial.println("**nomorefiles**");
      break;
    }
    //for (uint8_t i = 0; i < numTabs; i++) {
    //  Serial.print('\t');
    //}

    if(entry.isDirectory()) {
      //Serial.print("DIR\t");
      entries[entry_cnt].isDir = 1;
    } else {
      //Serial.print("FILE\t");
      entries[entry_cnt].isDir = 0;
    }
    //Serial.print(entry.name());
    
    if (entry.isDirectory()) {
      cx = snprintf ( buffer, 64, "%s", entry.name() );
	  entries[entry_cnt].fnamelen = cx;
	  //Serial.println(cx);
      for(int j =0;j<cx;j++) entries[entry_cnt].name[j] = buffer[j];
      //Serial.print(entries[entry_cnt].isDir);
      //Serial.print("  ");Serial.println(entries[entry_cnt].name);
	  entries[entry_cnt].size = 0;
      //printDirectory1(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      //Serial.print("\t\t");
      //Serial.println(entry.size(), DEC);
      cx = snprintf ( buffer, 64, "%s", entry.name() );
		entries[entry_cnt].fnamelen = cx;
		//Serial.print("NAME LEN: ");Serial.println(entries[entry_cnt].fnamelen);
      for(int j =0;j<cx;j++) entries[entry_cnt].name[j] = buffer[j];
      //Serial.print(entries[entry_cnt].isDir);
      //Serial.print(" ");Serial.print(entries[entry_cnt].name);
      entries[entry_cnt].size = entry.size();
      //Serial.print(" "); Serial.println( entries[entry_cnt].size);
    }
    entry.close();
  }
  //Serial.println(entry_cnt);
}

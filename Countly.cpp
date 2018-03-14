/*
 Countly.cpp
 CountlyCpp
 
 Created by Benoit Girard on 26/10/14.
 
 The MIT License (MIT)
 
 Copyright (c) 2015 Kontrol SAS (tanker.io)
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#include "Countly.h"
#ifndef WIN32
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#else
#include <time.h>
#include <Windows.h>
#include <cstdio>
#endif

#if TARGET_OS_IPHONE
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#include <iostream>
#define COUNTLY_EVENT_SEND_THRESHOLD 10
#define COUNTLY_DEFAULT_UPDATE_INTERVAL 60.0

#include "CountlyEventQueue.h"
#include "CountlyConnectionQueue.h"
using namespace std;

namespace CountlyCpp
{
  Countly * Countly::_instance = NULL;
  
  void * _startThreadTimer(void * obj)
  {
    Countly * ptr = (Countly *) obj;
    ptr->StartThreadTimer();
    return NULL;
  }
  
  Countly::Countly()
  {
    _eventQueue = new CountlyEventQueue();
    _connectionQueue = new CountlyConnectionQueue();
  }
  
  Countly::~Countly()
  {
    Stop();
    delete(_eventQueue);
    delete(_connectionQueue);
    _eventQueue = NULL;
    _connectionQueue = NULL;
  }
  
  Countly * Countly::GetInstance()
  {
    if (!_instance) _instance = new Countly();
    return _instance;
  } 
  
  void Countly::DeleteInstance()
  {
    if (!_instance) return;
    delete(_instance);
    _instance = NULL;
  }
  
  std::string Countly::GetVersion()
  {
    return COUNTLY_VERSION;
  }
 
  void Countly::SetMetrics(std::string os, std::string os_version, std::string device, std::string resolution, std::string carrier, std::string app_version)
  {

    _connectionQueue->SetMetrics(os, os_version, device, resolution, carrier, app_version);

  }
  
  
  void Countly::Stop()
  {
    if (!_threadRunning)
      return;
    _threadRunning = false;
	if (_thread.joinable())
		_thread.join();
  }
  
  void Countly::SetPath(std::string path)
  {
    _eventQueue->SetPath(path);
  }
  
  void Countly::Start(std::string appKey, std::string host, int port)
  {
    _connectionQueue->SetAppKey(appKey);
    _connectionQueue->SetAppHost(host, port);
	_thread = std::thread(&CountlyCpp::_startThreadTimer,this);
  }
  
  void Countly::StartOnCloud(std::string appKey)
  {
    Start(appKey, "https://cloud.count.ly", 80);
  }
  
  void Countly::RecordEvent(std::string key, int count)
  {
    _eventQueue->RecordEvent(key, count);
  }
  
  void Countly::RecordEvent(std::string key, int count, double sum)
  {
    _eventQueue->RecordEvent(key, count, sum);
  }
  
  void Countly::RecordEvent(std::string key, std::map<std::string, std::string> segmentation, int count)
  {
    _eventQueue->RecordEvent(key, segmentation, count);
  }
  
  void Countly::RecordEvent(std::string key, std::map<std::string, std::string> segmentation, int count, double sum)
  {
    _eventQueue->RecordEvent(key, segmentation, count, sum);
  }

  
  void Countly::StartThreadTimer()
  {
    int upd = 0;
    
    _threadRunning = true;
    bool lastUpd = false;
    while (_threadRunning)
    {
        //If not checked for too long or somehting was pending on last update
      if ((!upd) || (upd >= COUNTLY_DEFAULT_UPDATE_INTERVAL) || lastUpd)
      {
        lastUpd = _connectionQueue->UpdateSession(_eventQueue);
        upd = 0;
      }
      upd ++;

#ifndef WIN32
      sleep(1); //don't block for COUNTLY_DEFAULT_UPDATE_INTERVAL, it's really too long a pthread_join
#else
      Sleep(1000);
#endif
    }
  }

  
  
  unsigned long long Countly::GetTimestamp()
  {
    
      //times returns seconds from 1970 * CLK_TCK (nb of units per second)
      // *1000/CLK_TCK --> ms
#ifdef WIN32
    FILETIME lp;
    GetSystemTimeAsFileTime(&lp);
    unsigned long long res;
    res = lp.dwHighDateTime;
    res <<=32;
    res |= lp.dwLowDateTime;
    res /= 10;
    res -=  11644473600000000ULL;
    return res/1000;
#else
#ifdef __unix__
    struct timespec tms;
    if (clock_gettime(CLOCK_REALTIME,&tms))
      return -1;
    /* seconds, multiplied with 1 million */
    unsigned long long t = tms.tv_sec;
    t *= 1000;
	  /* Add full microseconds */
	  t += tms.tv_nsec/1000000;
    return t;
#else
#if TARGET_OS_IPHONE
    struct timespec ts;
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts.tv_sec = mts.tv_sec;
    ts.tv_nsec = mts.tv_nsec;
    /* seconds, multiplied with 1 million */
    unsigned long long t = ts.tv_sec;
    t *= 1000;
	  /* Add full microseconds */
	  t += ts.tv_nsec/1000000;
    return t;
#else
    struct tms buf;
    return times(&buf)*(1000.0/CLK_TCK);
#endif
#endif
#endif
  }
  
}

#!/bin/sh

# Kvaser CAN driver                     
# virtualcan.sh - start/stop virtualcan and create/delete device files
# 
#                 Copyright 2012 by Kvaser AB, M�lndal, Sweden
#                         http://www.kvaser.com
# 
#  This software is dual licensed under the following two licenses:
#  BSD-new and GPLv2. You may use either one. See the included
#  COPYING file for details.
# 
#  License: BSD-new
#  ===============================================================================
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#      * Redistributions of source code must retain the above copyright
#        notice, this list of conditions and the following disclaimer.
#      * Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#      * Neither the name of the <organization> nor the
#        names of its contributors may be used to endorse or promote products
#        derived from this software without specific prior written permission.
# 
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
#  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# 
#  License: GPLv2
#  ===============================================================================
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
# 
#  ---------------------------------------------------------------------------
#  

#     
# test kernel version
#     
#debug:
#set -x
kernel_major=`uname -r |cut -d \. -f 1` 
kernel_minor=`uname -r |cut -d \. -f 2` 

devicename=kvvirtualcan
LOG=`which logger`

if [ $kernel_major = 2 ] && [ $kernel_minor = 4 ]; then
  kv_module_install=insmod
else
  kv_module_install="modprobe -i"
fi

#
# install
#
case "$1" in
   start)
      /sbin/$kv_module_install $devicename || exit 1
      nrchan=`cat /proc/$devicename | grep 'total channels' | cut -d \  -f 3`
      major=`cat /proc/devices | grep ${devicename} | cut -d \  -f 1`
     rm -f /dev/${devicename}*
      for minor in $(seq 0 `expr $nrchan - 1`) ; do
         $LOG -t $0 "Created /dev/${devicename}${minor}"
         mknod /dev/${devicename}${minor} c ${major} ${minor}
      done
      ;;
   stop)
      /sbin/rmmod $devicename || exit 1
      rm -f /dev/${devicename}*
      $LOG -t $0 "Module $devicename removed"
      ;;
   *)
     printf "Usage: %s {start|stop}\n" $0
esac

#?set +x
exit 0 

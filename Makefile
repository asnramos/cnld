# Main Makefile for Code 'n Load daemon.
# Copyright (C) 2018  Pablo Ridolfi - Code 'n Load
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

PROGRAM = cnld

CNL_PATH = $(shell pwd)

SRC_PATH = src
INC_PATH = inc
OUT_PATH = out
OBJ_PATH = $(OUT_PATH)/obj

vpath %.c $(SRC_PATH)
vpath %.o $(OBJ_PATH)

SOURCES := $(wildcard $(SRC_PATH)/*.c)
OBJS := $(subst $(SRC_PATH),$(OBJ_PATH),$(SOURCES:.c=.o))
OBJ_FILES := $(notdir $(OBJS))

ifeq ($(DEBUG),)
CFLAGS := -Wall -fdata-sections -ffunction-sections -std=gnu99
else
CFLAGS := -g3 -Wall -fdata-sections -ffunction-sections -std=gnu99
endif

LFLAGS := -Wl,--gc-sections
LIBS := -lsystemd -lpthread -lm -lrt -lc

%.o: %.c
	$(CROSS_PREFIX)gcc $(CFLAGS) $(addprefix -I, $(INC_PATH)) -c $< -o $(OBJ_PATH)/$@
	$(CROSS_PREFIX)gcc $(CFLAGS) $(addprefix -I, $(INC_PATH)) -c $< -MM > $(OBJ_PATH)/$(@:.o=.d)

all: $(PROGRAM)

-include $(wildcard $(OBJ_PATH)/*.d)

$(PROGRAM): $(OBJ_FILES)
	$(CROSS_PREFIX)gcc $(LIBPATH) $(OBJS) $(LFLAGS) -o $(OUT_PATH)/$(PROGRAM) $(LIBS)
	$(CROSS_PREFIX)size $(OUT_PATH)/$(PROGRAM)

clean:
	rm -f $(OBJS) $(OUT_PATH)/$(PROGRAM) $(OBJ_PATH)/*.d

update: $(PROGRAM)
	sudo systemctl stop cnld
	sudo cp out/cnld /usr/bin/
	sudo systemctl start cnld

install:
	@echo "***********************************"
	@echo "*** Building server application ***"
	@echo "***********************************"
	make $(PROGRAM)
	@echo "*********************"
	@echo "*** Running setup ***"
	@echo "*********************"
	$(CNL_PATH)/etc/setup.sh

uninstall:
	-sudo systemctl stop cnld
	-sudo systemctl stop cnl_app
	-sudo systemctl stop cnl_ssh
	-sudo systemctl disable cnld
	-sudo systemctl disable cnl_app
	-sudo systemctl disable cnl_ssh
	sudo rm -f /usr/bin/cnld /usr/bin/cnl_app /usr/bin/cnl
	sudo rm -f /etc/systemd/system/cnl_app.service /etc/systemd/system/cnld.service /etc/systemd/system/cnl_ssh.service /etc/systemd/system/cnld.socket

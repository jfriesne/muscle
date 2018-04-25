EXECUTABLES = \
	example_1_dumb_server             \
	example_2_dumb_client             \
	example_3_annotated_dumb_server   \
	example_4_smart_server            \
	example_5_smart_client            \
	example_6_smart_server_with_pulsenode \
	example_7_smart_server_with_udp_pingpong

MUSCLE_DIR = ../../../..
MUSCLE_LIB = $(MUSCLE_DIR)/server/libmuscle.a

CXXFLAGS   = -I$(MUSCLE_DIR) -std=c++11 -O3 -DMUSCLE_ENABLE_ZLIB_ENCODING
LFLAGS    += -L$(MUSCLE_DIR)/server
OBJS       = $(EXECUTABLES:=.o)

ifeq ($(OSTYPE),)
  OSTYPE = $(strip $(shell uname))
endif

ifeq ($(OSTYPE),Darwin) 
   LIBS += -framework IOKit -framework Carbon -framework SystemConfiguration
   CXXFLAGS += -stdlib=libc++
endif 

ifeq ($(OSTYPE),linux) 
   LIBS += -lutil -pthread
endif 
ifeq ($(OSTYPE),Linux) 
   LIBS += -lutil -pthread
endif 

all:  $(MUSCLE_LIB) $(EXECUTABLES)

$(MUSCLE_LIB):
	cd $(MUSCLE_DIR)/server && make

$(EXECUTABLES) : % : $(MUSCLE_LIB) $(OBJS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) -o $@ $@.cpp $(MUSCLE_LIB) $(LIBS)

clean:
	rm -f *.o *.xSYM $(EXECUTABLES)

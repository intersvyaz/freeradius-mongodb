TARGET		= rlm_mongodb_ops
SRCS		= rlm_mongodb_ops.c
RLM_CFLAGS	= -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0
RLM_LIBS	= -lmongoc-1.0

include ../rules.mak



CC = gcc
CFLAGS = -g
RM = rm

EXEM = oss 
EXES = userProcess 
SRCSM = oss.c semaphore.c
SRCSS = userProcess.c semaphore.c
OBJSM = ${SRCSM:.c=.o}
OBJSS = ${SRCSS:.c=.o}

.c:.o
	$(CC) $(CFLAGS) -c $<

all : $(EXEM) $(EXES)

$(EXEM) : $(OBJSM)
	$(CC) -o $@ $(OBJSM)

$(OBJSM) : oss.h semaphore.h

$(EXES) : $(OBJSS)
	$(CC) -o $@ $(OBJSS)

$(OBJSS) : userProcess.h semaphore.h

clean :
	$(RM) -f $(EXES) $(EXEM) $(OBJSS) $(OBJSM)




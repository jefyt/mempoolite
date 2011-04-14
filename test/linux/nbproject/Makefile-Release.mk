#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=
AS=as

# Macros
CND_PLATFORM=GNU-Linux-x86
CND_CONF=Release
CND_DISTDIR=dist

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=build/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/_ext/1472/test.o \
	${OBJECTDIR}/_ext/1445274692/mempoolite.o


# C Compiler Flags
CFLAGS=-Wextra

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-lpthread

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-Release.mk dist/Release/test

dist/Release/test: ${OBJECTFILES}
	${MKDIR} -p dist/Release
	${LINK.c} -o ${CND_DISTDIR}/${CND_CONF}/test -s ${OBJECTFILES} ${LDLIBSOPTIONS} 

${OBJECTDIR}/_ext/1472/test.o: ../test.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1472
	${RM} $@.d
	$(COMPILE.c) -O2 -Wall -s -DMEMPOOLITE_ENABLED=1 -I../../inc -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1472/test.o ../test.c

${OBJECTDIR}/_ext/1445274692/mempoolite.o: ../../src/mempoolite.c 
	${MKDIR} -p ${OBJECTDIR}/_ext/1445274692
	${RM} $@.d
	$(COMPILE.c) -O2 -Wall -s -DMEMPOOLITE_ENABLED=1 -I../../inc -MMD -MP -MF $@.d -o ${OBJECTDIR}/_ext/1445274692/mempoolite.o ../../src/mempoolite.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r build/Release
	${RM} dist/Release/test

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc

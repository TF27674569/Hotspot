################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/jdk/windows/native/sun/awt/splashscreen/splashscreen_sys.c 

OBJS += \
./src/jdk/windows/native/sun/awt/splashscreen/splashscreen_sys.o 

C_DEPS += \
./src/jdk/windows/native/sun/awt/splashscreen/splashscreen_sys.d 


# Each subdirectory must supply rules for building sources it contributes
src/jdk/windows/native/sun/awt/splashscreen/%.o: ../src/jdk/windows/native/sun/awt/splashscreen/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/jdk/windows/native/sun/security/pkcs11/wrapper/p11_md.c 

OBJS += \
./src/jdk/windows/native/sun/security/pkcs11/wrapper/p11_md.o 

C_DEPS += \
./src/jdk/windows/native/sun/security/pkcs11/wrapper/p11_md.d 


# Each subdirectory must supply rules for building sources it contributes
src/jdk/windows/native/sun/security/pkcs11/wrapper/%.o: ../src/jdk/windows/native/sun/security/pkcs11/wrapper/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '



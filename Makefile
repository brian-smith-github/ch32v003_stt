all : flash

TARGET:=stt

include ../../ch32v003fun/ch32v003fun.mk

flash : cv_flash
clean : cv_clean



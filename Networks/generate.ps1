# Generate object detection model
stedgeai generate --model models/st_yolo_x_nano_480_1.0_0.25_3_int8.tflite --target stm32n6 --st-neural-art object_detection@my_neural_art.json --name od_yolo_x_person

Copy-Item -Path "st_ai_output/od_yolo_x_person_ecblobs.h" -Destination "./Src"
Copy-Item -Path "st_ai_output/od_yolo_x_person.c" -Destination "./Src"
Copy-Item -Path "st_ai_output/od_yolo_x_person.h" -Destination "./Src"

Copy-Item -Path "st_ai_output/od_yolo_x_person_atonbuf.xSPI2.raw" -Destination "./Bin/od_yolo_x_person_atonbuf.xSPI2.bin"
arm-none-eabi-objcopy -I binary "./Bin/od_yolo_x_person_atonbuf.xSPI2.bin" --change-addresses 0x70380000 -O ihex "./Bin/od_yolo_x_person_atonbuf.hex"
#include "AudioPlayer.h"

File WavFile;
struct WavHeader_Struct WavHeader;

const char* wavFiles[] = { "/1.wav", "/2.wav", "/3.wav", "/4.wav", "/5.wav" };
int currentFileIndex = 0;
const int numFiles = sizeof(wavFiles) / sizeof(wavFiles[0]);

static const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100, // Default sample rate, will be updated later
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll=0,
    .tx_desc_auto_clear= true,
    .fixed_mclk=-1    
};

static const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
};

void setupAudio() {
    Serial.begin(115200);
    SDCardInit();
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);

    playCurrentFile();
}

void playCurrentFile() {
    WavFile = SD.open(wavFiles[currentFileIndex]);
    
    if (!WavFile) {
        Serial.printf("Could not open '%s'\n", wavFiles[currentFileIndex]);
        return;
    }
    
    WavFile.read((byte*)&WavHeader, 44);
    DumpWAVHeader(&WavHeader);

    if (ValidWavData(&WavHeader)) {
        uint32_t adjustedSampleRate = WavHeader.SampleRate / 2; // Slow down by half
        Serial.printf("Setting sample rate to %u (original: %u)\n", adjustedSampleRate, WavHeader.SampleRate);
        i2s_set_sample_rates(I2S_NUM, adjustedSampleRate);
    }
}

void loopAudio() {
    PlayWav();
}

void PlayWav() {
    static bool ReadingFile = true;
    static byte Samples[NUM_BYTES_TO_READ_FROM_FILE];
    static uint16_t BytesRead;

    if (ReadingFile) {
        BytesRead = ReadFile(Samples);
        ReadingFile = false;
    } else {
        ReadingFile = FillI2SBuffer(Samples, BytesRead);
    }
}

uint16_t ReadFile(byte* Samples) {
    static uint32_t BytesReadSoFar = 0;
    uint16_t BytesToRead;

    if (BytesReadSoFar + NUM_BYTES_TO_READ_FROM_FILE > WavHeader.DataSize) {
        BytesToRead = WavHeader.DataSize - BytesReadSoFar;
    } else {
        BytesToRead = NUM_BYTES_TO_READ_FROM_FILE;
    }
    
    WavFile.read(Samples, BytesToRead);
    BytesReadSoFar += BytesToRead;

    if (BytesReadSoFar >= WavHeader.DataSize) {
        // Close the current file and move to the next one
        delay(2000);
        WavFile.close();
        BytesReadSoFar = 0;
        nextFile();
    }
    return BytesToRead;
}

void nextFile() {
    // Increment file index and loop back to the beginning if needed
    currentFileIndex = (currentFileIndex + 1) % numFiles;
    playCurrentFile();
}

bool FillI2SBuffer(byte* Samples, uint16_t BytesInBuffer) {
    size_t BytesWritten;
    static uint16_t BufferIdx = 0;
    uint8_t* DataPtr = Samples + BufferIdx;
    uint16_t BytesToSend = BytesInBuffer - BufferIdx;
    if (i2s_write(I2S_NUM, DataPtr, BytesToSend, &BytesWritten, portMAX_DELAY) == ESP_OK) {
        BufferIdx += BytesWritten;
    } else {
        Serial.println("Error writing to I2S");
    }

    if (BufferIdx >= BytesInBuffer) {
        BufferIdx = 0;
        return true;
    } else {
        return false;
    }
}


void SDCardInit() {        
    pinMode(SD_CS, OUTPUT); 
    digitalWrite(SD_CS, HIGH);
    if (!SD.begin(SD_CS)) {
        Serial.println("Error talking to SD card!");
        while (true);
    }
}

bool ValidWavData(struct WavHeader_Struct* Wav) {
    if (memcmp(Wav->RIFFSectionID, "RIFF", 4) != 0) {    
        Serial.println("Invalid data - Not RIFF format");
        return false;
    }
    if (memcmp(Wav->RiffFormat, "WAVE", 4) != 0) {
        Serial.println("Invalid data - Not Wave file");
        return false;
    }
    if (memcmp(Wav->FormatSectionID, "fmt", 3) != 0) {
        Serial.println("Invalid data - No format section found");
        return false;
    }
    if (memcmp(Wav->DataSectionID, "data", 4) != 0) {
        Serial.println("Invalid data - data section not found");
        return false;
    }
    if (Wav->FormatID != 1) {
        Serial.println("Invalid data - format Id must be 1");
        return false;
    }
    if (Wav->FormatSize != 16) {
        Serial.println("Invalid data - format section size must be 16.");
        return false;
    }
    if ((Wav->NumChannels != 1) && (Wav->NumChannels != 2)) {
        Serial.println("Invalid data - only mono or stereo permitted.");
        return false;
    }
    if (Wav->SampleRate > 48000) {
        Serial.println("Invalid data - Sample rate cannot be greater than 48000");
        return false;
    }
    if ((Wav->BitsPerSample != 8) && (Wav->BitsPerSample != 16)) {
        Serial.println("Invalid data - Only 8 or 16 bits per sample permitted.");
        return false;
    }
    return true;
}

void DumpWAVHeader(struct WavHeader_Struct* Wav) {
    Serial.print("RIFFSectionID: ");
    PrintData(Wav->RIFFSectionID, 4);
    Serial.print("Size: ");
    Serial.println(Wav->Size);
    Serial.print("RiffFormat: ");
    PrintData(Wav->RiffFormat, 4);
    Serial.print("FormatSectionID: ");
    PrintData(Wav->FormatSectionID, 4);
    Serial.print("FormatSize: ");
    Serial.println(Wav->FormatSize);
    Serial.print("FormatID: ");
    Serial.println(Wav->FormatID);
    Serial.print("NumChannels: ");
    Serial.println(Wav->NumChannels);
    Serial.print("SampleRate: ");
    Serial.println(Wav->SampleRate);
    Serial.print("ByteRate: ");
    Serial.println(Wav->ByteRate);
    Serial.print("BlockAlign: ");
    Serial.println(Wav->BlockAlign);
    Serial.print("BitsPerSample: ");
    Serial.println(Wav->BitsPerSample);
    Serial.print("DataSectionID: ");
    PrintData(Wav->DataSectionID, 4);
    Serial.print("DataSize: ");
    Serial.println(Wav->DataSize);
}

void PrintData(const char* Data, uint8_t NumBytes) {
    for (uint8_t i = 0; i < NumBytes; i++) {
        Serial.print(Data[i]); 
    }
    Serial.println();
}









// #include "AudioPlayer.h"

// File WavFile;
// struct WavHeader_Struct WavHeader;

// static const i2s_config_t i2s_config = {
//     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
//     .sample_rate = 44100, // Default sample rate, will be updated later
//     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
//     .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
//     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//     .dma_buf_count = 8,
//     .dma_buf_len = 64,
//     .use_apll=0,
//     .tx_desc_auto_clear= true,
//     .fixed_mclk=-1    
// };

// static const i2s_pin_config_t pin_config = {
//     .bck_io_num = I2S_BCLK,
//     .ws_io_num = I2S_LRC,
//     .data_out_num = I2S_DOUT,
//     .data_in_num = I2S_PIN_NO_CHANGE
// };

// void setupAudio() {
//     Serial.begin(115200);
//     SDCardInit();
//     i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
//     i2s_set_pin(I2S_NUM, &pin_config);

//     // WavFile = SD.open("/StarWars60.wav");
//     WavFile = SD.open("/StarWars60.wav");

//     // 1.wav, 2.wav, 3.wav, 4.wav, 5.wav

//     if (!WavFile) {
//         Serial.println("Could not open 'StarWars60.wav'");
//         return;
//     }
    
//     WavFile.read((byte*)&WavHeader, 44);
//     DumpWAVHeader(&WavHeader);

//     if (ValidWavData(&WavHeader)) {
//         uint32_t adjustedSampleRate = WavHeader.SampleRate / 2; // Slow down by half
//         // uint32_t adjustedSampleRate = WavHeader.SampleRate; // Slow down by half
//         Serial.printf("Setting sample rate to %u (original: %u)\n", adjustedSampleRate, WavHeader.SampleRate);
//         i2s_set_sample_rates(I2S_NUM, adjustedSampleRate);
//     }
// }

// void loopAudio() {
//     PlayWav();
// }

// void PlayWav() {
//     static bool ReadingFile = true;
//     static byte Samples[NUM_BYTES_TO_READ_FROM_FILE];
//     static uint16_t BytesRead;

//     if (ReadingFile) {
//         BytesRead = ReadFile(Samples);
//         ReadingFile = false;
//     } else {
//         ReadingFile = FillI2SBuffer(Samples, BytesRead);
//     }
// }

// uint16_t ReadFile(byte* Samples) {
//     static uint32_t BytesReadSoFar = 0;
//     uint16_t BytesToRead;

//     if (BytesReadSoFar + NUM_BYTES_TO_READ_FROM_FILE > WavHeader.DataSize) {
//         BytesToRead = WavHeader.DataSize - BytesReadSoFar;
//     } else {
//         BytesToRead = NUM_BYTES_TO_READ_FROM_FILE;
//     }
    
//     WavFile.read(Samples, BytesToRead);
//     BytesReadSoFar += BytesToRead;

//     if (BytesReadSoFar >= WavHeader.DataSize) {
//         WavFile.seek(44);
//         BytesReadSoFar = 0;
//     }
//     return BytesToRead;
// }

// bool FillI2SBuffer(byte* Samples, uint16_t BytesInBuffer) {
//     size_t BytesWritten;
//     static uint16_t BufferIdx = 0;
//     uint8_t* DataPtr = Samples + BufferIdx;
//     uint16_t BytesToSend = BytesInBuffer - BufferIdx;
//     i2s_write(I2S_NUM, DataPtr, BytesToSend, &BytesWritten, 1);
//     BufferIdx += BytesWritten;

//     if (BufferIdx >= BytesInBuffer) {
//         BufferIdx = 0;
//         return true;
//     } else {
//         return false;
//     }
// }

// void SDCardInit() {        
//     pinMode(SD_CS, OUTPUT); 
//     digitalWrite(SD_CS, HIGH);
//     if (!SD.begin(SD_CS)) {
//         Serial.println("Error talking to SD card!");
//         while (true);
//     }
// }

// bool ValidWavData(struct WavHeader_Struct* Wav) {
//     if (memcmp(Wav->RIFFSectionID, "RIFF", 4) != 0) {    
//         Serial.println("Invalid data - Not RIFF format");
//         return false;
//     }
//     if (memcmp(Wav->RiffFormat, "WAVE", 4) != 0) {
//         Serial.println("Invalid data - Not Wave file");
//         return false;
//     }
//     if (memcmp(Wav->FormatSectionID, "fmt", 3) != 0) {
//         Serial.println("Invalid data - No format section found");
//         return false;
//     }
//     if (memcmp(Wav->DataSectionID, "data", 4) != 0) {
//         Serial.println("Invalid data - data section not found");
//         return false;
//     }
//     if (Wav->FormatID != 1) {
//         Serial.println("Invalid data - format Id must be 1");
//         return false;
//     }
//     if (Wav->FormatSize != 16) {
//         Serial.println("Invalid data - format section size must be 16.");
//         return false;
//     }
//     if ((Wav->NumChannels != 1) && (Wav->NumChannels != 2)) {
//         Serial.println("Invalid data - only mono or stereo permitted.");
//         return false;
//     }
//     if (Wav->SampleRate > 48000) {
//         Serial.println("Invalid data - Sample rate cannot be greater than 48000");
//         return false;
//     }
//     if ((Wav->BitsPerSample != 8) && (Wav->BitsPerSample != 16)) {
//         Serial.println("Invalid data - Only 8 or 16 bits per sample permitted.");
//         return false;
//     }
//     return true;
// }

// void DumpWAVHeader(struct WavHeader_Struct* Wav) {
//     Serial.print("RIFFSectionID: ");
//     PrintData(Wav->RIFFSectionID, 4);
//     Serial.print("Size: ");
//     Serial.println(Wav->Size);
//     Serial.print("RiffFormat: ");
//     PrintData(Wav->RiffFormat, 4);
//     Serial.print("FormatSectionID: ");
//     PrintData(Wav->FormatSectionID, 4);
//     Serial.print("FormatSize: ");
//     Serial.println(Wav->FormatSize);
//     Serial.print("FormatID: ");
//     Serial.println(Wav->FormatID);
//     Serial.print("NumChannels: ");
//     Serial.println(Wav->NumChannels);
//     Serial.print("SampleRate: ");
//     Serial.println(Wav->SampleRate);
//     Serial.print("ByteRate: ");
//     Serial.println(Wav->ByteRate);
//     Serial.print("BlockAlign: ");
//     Serial.println(Wav->BlockAlign);
//     Serial.print("BitsPerSample: ");
//     Serial.println(Wav->BitsPerSample);
//     Serial.print("DataSectionID: ");
//     PrintData(Wav->DataSectionID, 4);
//     Serial.print("DataSize: ");
//     Serial.println(Wav->DataSize);
// }

// void PrintData(const char* Data, uint8_t NumBytes) {
//     for (uint8_t i = 0; i < NumBytes; i++) {
//         Serial.print(Data[i]); 
//     }
//     Serial.println();
// }

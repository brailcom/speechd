typedef enum {                  // type of voice
        MALE = 0,               // (numbers divisible by 2 should mean male)
        FEMALE = 1,
        CHILD_MALE = 2,
        CHILD_FEMALE = 3
}EVoiceType;

typedef struct{
        int fd;
        int paused;
        int priority;           // priority between 1 and 3
        int punctuation_mode;   // this will of course not be integer
        int speed;              // speed: 100 = normal, ???
        float pitch;            // pitch: ???
        char *client_name;
        char *language;         // language: default = english
        char *output_module;    // output module: festival, odmluva, epos,...
        EVoiceType voice_type;  // see EVoiceType definition
        int spelling;           // spelling mode: 0 -- off, 1 -- on
        int cap_let_recogn;     // cap. letters recogn.: 0 -- off, 1 -- on
}TFDSetElement;


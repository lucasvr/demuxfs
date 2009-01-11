#ifndef __stream_type_h
#define __stream_type_h

bool stream_type_is_mpe(uint8_t stream_type);
bool stream_type_is_video(uint8_t stream_type);
bool stream_type_is_audio(uint8_t stream_type);
bool stream_type_is_event_message(uint8_t stream_type);
bool stream_type_is_data_carousel(uint8_t stream_type);
bool stream_type_is_object_carousel(uint8_t stream_type);
const char *stream_type_to_string(uint8_t stream_type);

#endif /* __stream_type_h */

#ifndef __component_tag_h
#define __component_tag_h

bool component_is_video(uint8_t component_tag, bool *is_primary);
bool component_is_audio(uint8_t component_tag, bool *is_primary);
bool component_is_caption(uint8_t component_tag, bool *is_primary);
bool component_is_superimposed(uint8_t component_tag, bool *is_primary);
bool component_is_object_carousel(uint8_t component_tag, bool *is_primary);
bool component_is_event_message(uint8_t component_tag);
bool component_is_data_carousel(uint8_t component_tag, bool *is_primary);
bool component_is_one_seg(uint8_t component_tag);

#endif /* __component_tag_h */

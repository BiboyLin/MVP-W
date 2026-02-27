#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

/**
 * Set text on display
 * @param text Text to display
 * @param font_size Font size
 * @return 0 on success, -1 on error
 */
int hal_display_set_text(const char *text, int font_size);

/**
 * Set emoji image on display
 * @param emoji_id Emoji type ID (emoji_type_t)
 * @return 0 on success, -1 on error
 */
int hal_display_set_emoji(int emoji_id);

/**
 * Initialize display hardware
 */
int hal_display_init(void);

#endif /* HAL_DISPLAY_H */

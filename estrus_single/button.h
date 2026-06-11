#pragma once

enum ButtonEvent {

    BTN_NONE,

    BTN_SINGLE_CLICK,

    BTN_DOUBLE_CLICK,

    BTN_LONG_PRESS
};

void initButton();
void buttonTask(void *pv);
ButtonEvent getButtonEvent();

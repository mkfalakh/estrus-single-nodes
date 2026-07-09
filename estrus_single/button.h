#pragma once

enum ButtonEvent {

    BTN_NONE,

    BTN_SINGLE_CLICK,

    BTN_DOUBLE_CLICK
};

void initButton();
void buttonTask(void *pv);
ButtonEvent getButtonEvent();

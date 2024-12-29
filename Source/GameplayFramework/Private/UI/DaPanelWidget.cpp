// Copyright Dream Awake Solutions LLC


#include "UI/DaPanelWidget.h"

void UDaPanelWidget::NativeDestruct()
{
	Super::NativeDestruct();

	OnPanelClosed.Broadcast();
}

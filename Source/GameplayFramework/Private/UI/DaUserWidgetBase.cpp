// Copyright Dream Awake Solutions LLC


#include "UI/DaUserWidgetBase.h"

void UDaUserWidgetBase::SetWidgetController(UObject* InWidgetController)
{
	WidgetController = InWidgetController;
	NativeWidgetControllerSet();
	
}

void UDaUserWidgetBase::NativeWidgetControllerSet()
{
	WidgetControllerSet();
}

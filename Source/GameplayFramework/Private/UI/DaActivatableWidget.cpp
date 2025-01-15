// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/DaActivatableWidget.h"

#include "Editor/WidgetCompilerLog.h"

#define LOCTEXT_NAMESPACE "DaActivatableWidget"

UDaActivatableWidget::UDaActivatableWidget(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

TOptional<FUIInputConfig> UDaActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputConfig)
	{
	case EDaWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case EDaWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case EDaWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case EDaWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

#if WITH_EDITOR
void UDaActivatableWidget::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree,
	class IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	if (!GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UDaActivatableWidget, BP_GetDesiredFocusTarget)))
	{
		if (GetParentNativeClass(GetClass()) == UDaActivatableWidget::StaticClass())
		{
			CompileLog.Warning(LOCTEXT("ValidateGetDesiredFocusTarget_Warning", "GetDesiredFocusTarget wasn't implemented, you're going to have trouble using gamepads on this screen."));
		}
		else
		{
			//TODO - Note for now, because we can't guarantee it isn't implemented in a native subclass of this one.
			CompileLog.Note(LOCTEXT("ValidateGetDesiredFocusTarget_Note", "GetDesiredFocusTarget wasn't implemented, you're going to have trouble using gamepads on this screen.  If it was implemented in the native base class you can ignore this message."));
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
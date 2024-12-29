// Copyright Dream Awake Solutions LLC


#include "UI/DaOverlayWidgetBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "UI/DaMessageWidget.h"
#include "UI/DaWidgetMessageData.h"

void UDaOverlayWidgetBase::NativeWidgetControllerSet()
{
	if (UDaOverlayWidgetController* OverlayWidgetController = GetOverlayWidgetController())
	{
		OverlayWidgetController->MessageWidgetDataDelegate.AddDynamic(this, &ThisClass::OnMessageDataReceived);
	}
	
	Super::NativeWidgetControllerSet();
}

void UDaOverlayWidgetBase::OnMessageDataReceived(const FDaUIWidgetMessageData& Data)
{
	if (Data.MessageWidget != nullptr)
	{
		if (UUserWidget* Widget = CreateWidgetInstance(*this, Data.MessageWidget, Data.MessageTag.GetTagName()))
		{
			if (UDaMessageWidget* MessageWidget = Cast<UDaMessageWidget>(Widget))
			{
				MessageWidget->NativeSetImageAndText(Data.Image, Data.Message);

				// position in the center of the viewport
				FVector2d Size = UWidgetLayoutLibrary::GetViewportSize(this);
				MessageWidget->SetPositionInViewport(FVector2D(Size.X * 0.5f, Size.Y * 0.5f));

				MessageWidget->AddToViewport();
			}
		}
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "CECollectibleData.h"
#include "CECollectibleViewModel.h"


UCECollectibleData::UCECollectibleData()
{
	CollectibleViewModeClass = UCECollectibleViewModel::StaticClass();
}

void UCECollectibleData::SetPlayerSelectedYear(int32 Year)
{
	AppraisalData.PlayerSetYear = Year;
	CollectibleViewModel->SetPlayerSetYear(AppraisalData.PlayerSetYear);
}

void UCECollectibleData::SetPlayerSelectedGrade(int32 Grade)
{
	AppraisalData.PlayerSetGrade = Grade;
	CollectibleViewModel->SetPlayerSetGrade(AppraisalData.PlayerSetGrade);
}

void UCECollectibleData::AddPlayerSelectedSpecialTag(const FGameplayTag& InTag)
{
	AppraisalData.PlayerSetSpecialTags.AddTag(InTag);
	CollectibleViewModel->SetPlayerSetSpecialTags(AppraisalData.PlayerSetSpecialTags);
}

void UCECollectibleData::RemovePlayerSelectedSpecialTag(const FGameplayTag& InTag)
{
	AppraisalData.PlayerSetSpecialTags.RemoveTag(InTag);
	CollectibleViewModel->SetPlayerSetSpecialTags(AppraisalData.PlayerSetSpecialTags);
}

void UCECollectibleData::SetAppraisedValue(int32 Value)
{
	AppraisalData.AppraisedValue = Value;
	CollectibleViewModel->SetAppraisedValue(AppraisalData.AppraisedValue);
}

void UCECollectibleData::SetupViewModel()
{
	CollectibleViewModel = NewObject<UCECollectibleViewModel>(this, CollectibleViewModeClass);
	UE_LOG(LogTemp, Warning, TEXT("Setting up View Model %s"), *CollectibleViewModel->GetClass()->GetName());

	// Copy data from CollectibleDataRef
	CollectibleViewModel->SetCollectibleName(CollectibleDataRef.CollectibleName);
	CollectibleViewModel->SetGrade(CollectibleDataRef.Grade);

	FGameplayTagContainer AllSpecialTags;
	AllSpecialTags.AppendTags(CollectibleDataRef.SpecialTags);

	if (CoreTypeTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Collectibles.Core.Coin"))))
	{
		// Copy data from Coin Template 
		AllSpecialTags.AppendTags(CoinTemplateData.SpecialTags);
		CollectibleViewModel->SetSubtype(CoinTemplateData.Subtype);
		CollectibleViewModel->SetYear(CoinTemplateData.Year);
		CollectibleViewModel->SetEdition(CoinTemplateData.Edition.ToString());
		CollectibleViewModel->SetIssuer(CoinTemplateData.Issuer.ToString());
	} else if (CoreTypeTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Collectibles.Core.Card"))))
	{
		// Copy data from Card Template 
		AllSpecialTags.AppendTags(CardTemplateData.SpecialTags);
		CollectibleViewModel->SetSubtype(CardTemplateData.Subtype);
		CollectibleViewModel->SetYear(CardTemplateData.Year);
		CollectibleViewModel->SetEdition(CardTemplateData.Edition.ToString());
		CollectibleViewModel->SetIssuer(CardTemplateData.Issuer.ToString());
	}
	CollectibleViewModel->SetSpecialTags(AllSpecialTags);

	// Copy data from AppraisalData
	if (AppraisalData.IsAppraisalValid())
	{
		CollectibleViewModel->SetPlayerSetGrade(AppraisalData.PlayerSetGrade);
		CollectibleViewModel->SetPlayerSetYear(AppraisalData.PlayerSetYear);
		CollectibleViewModel->SetPlayerSetSpecialTags(AppraisalData.PlayerSetSpecialTags);
		CollectibleViewModel->SetAppraisedValue(AppraisalData.AppraisedValue);
	}
}

UTexture2D* UCECollectibleData::GetIcon() const
{
	if (CoreTypeTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Collectibles.Core.Coin"))))
	{
		return CoinTemplateData.Image.LoadSynchronous();
	}
	else if (CoreTypeTag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Collectibles.Core.Card"))))
	{
		return CardTemplateData.Image.LoadSynchronous();
	}
	return nullptr;
}



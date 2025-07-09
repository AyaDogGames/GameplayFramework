#include "Inventory/DaMcpInventoryServerSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

void UDaMcpInventoryServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UDaMcpInventoryServerSubsystem::AddItemToServer(AActor* Owner, const FDaInventoryItemData& ItemData)
{
    FString Url = TEXT("http://localhost:8080/api/inventory/add");
    FString Payload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("owner"), Owner ? Owner->GetName() : TEXT("None"));
    Writer->WriteValue(TEXT("item"), ItemData.ItemName.ToString());
    Writer->WriteValue(TEXT("id"), ItemData.ItemID.ToString());
    Writer->WriteObjectEnd();
    Writer->Close();

    SendRequest(Url, TEXT("POST"), Payload);
}

void UDaMcpInventoryServerSubsystem::RemoveItemFromServer(AActor* Owner, const FGuid& ItemID)
{
    FString Url = TEXT("http://localhost:8080/api/inventory/remove");
    FString Payload;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("owner"), Owner ? Owner->GetName() : TEXT("None"));
    Writer->WriteValue(TEXT("id"), ItemID.ToString());
    Writer->WriteObjectEnd();
    Writer->Close();

    SendRequest(Url, TEXT("POST"), Payload);
}

void UDaMcpInventoryServerSubsystem::QueryInventoryFromServer(AActor* Owner)
{
    FString Url = FString::Printf(TEXT("http://localhost:8080/api/inventory/query?owner=%s"), Owner ? *Owner->GetName() : TEXT("None"));
    SendRequest(Url, TEXT("GET"), FString());
}

void UDaMcpInventoryServerSubsystem::SendRequest(const FString& Url, const FString& Verb, const FString& Payload)
{
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(Verb);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    if (!Payload.IsEmpty())
    {
        Request->SetContentAsString(Payload);
    }
    Request->ProcessRequest();
}


# Migration Guide: UPawnSensingComponent to UAIPerceptionComponent

## Overview

In Unreal Engine 5.5, `UPawnSensingComponent` has been deprecated in favor of the more robust `UAIPerceptionComponent`. This document outlines the migration process for the GameplayFramework plugin's `ADaAICharacter` class.

## Deprecation Details

**Affected Files:**
- `/Plugins/GameplayFramework/Source/GameplayFramework/Public/AI/DaAICharacter.h`
- `/Plugins/GameplayFramework/Source/GameplayFramework/Private/AI/DaAICharacter.cpp`

**Deprecation Warnings:**
- Line 26: `PawnSensingComp = CreateDefaultSubobject<UPawnSensingComponent>("PawnSensingComp");`
- Line 47: `PawnSensingComp->OnSeePawn.AddDynamic(this, &ADaAICharacter::OnPawnSeen);`

## Current Implementation Analysis

The current `ADaAICharacter` uses `UPawnSensingComponent` for:
1. **Visual Detection**: Detecting when pawns enter the AI's field of view
2. **Target Setting**: Automatically setting detected pawns as blackboard targets
3. **UI Feedback**: Displaying a widget when the player is spotted
4. **Combat Integration**: Switching targets when damaged by another actor

## Migration Steps

### Step 1: Update Header File (DaAICharacter.h)

**Replace includes:**
```cpp
// OLD
#include "Perception/PawnSensingComponent.h"

// NEW
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
```

**Replace component declaration:**
```cpp
// OLD (line 28-29)
UPROPERTY(VisibleAnywhere, Category="Components")
TObjectPtr<UPawnSensingComponent> PawnSensingComp;

// NEW
UPROPERTY(VisibleAnywhere, Category="Components")
TObjectPtr<UAIPerceptionComponent> AIPerceptionComp;

UPROPERTY(EditDefaultsOnly, Category="AI")
TObjectPtr<UAISenseConfig_Sight> SightConfig;
```

**Update function signature:**
```cpp
// OLD (line 55-56)
UFUNCTION()
void OnPawnSeen(APawn* Pawn);

// NEW
UFUNCTION()
void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
```

### Step 2: Update Implementation File (DaAICharacter.cpp)

**Update includes:**
```cpp
// Add at top with other includes
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
```

**Update constructor:**
```cpp
// OLD (line 26)
PawnSensingComp = CreateDefaultSubobject<UPawnSensingComponent>("PawnSensingComp");

// NEW
// Create AI Perception Component
AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>("AIPerceptionComp");
AIPerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());

// Configure sight sense
SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>("SightConfig");
SightConfig->SightRadius = 3000.0f;
SightConfig->LoseSightRadius = 3500.0f;
SightConfig->PeripheralVisionAngleDegrees = 90.0f;
SightConfig->SetMaxAge(5.0f);
SightConfig->DetectionByAffiliation.bDetectEnemies = true;
SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

// Add sight configuration to perception component
AIPerceptionComp->ConfigureSense(*SightConfig);
```

**Update PostInitializeComponents:**
```cpp
// OLD (line 47)
PawnSensingComp->OnSeePawn.AddDynamic(this, &ADaAICharacter::OnPawnSeen);

// NEW
AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ADaAICharacter::OnTargetPerceptionUpdated);
```

**Replace OnPawnSeen function:**
```cpp
// OLD (lines 68-78)
void ADaAICharacter::OnPawnSeen(APawn* Pawn)
{
    if (GetTargetActor() == Pawn)
    {
        return;
    }
    
    SetTargetActor(Pawn);
    MulticastOnPawnSeen(Pawn);
}

// NEW
void ADaAICharacter::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    // Check if this is a sight stimulus and the actor was successfully sensed
    if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>() && Stimulus.WasSuccessfullySensed())
    {
        APawn* SensedPawn = Cast<APawn>(Actor);
        if (SensedPawn && GetTargetActor() != SensedPawn)
        {
            SetTargetActor(SensedPawn);
            MulticastOnPawnSeen(SensedPawn);
        }
    }
}
```

### Step 3: Additional Considerations

#### AI Controller Setup

The AI controller may need to register as a perception listener:
```cpp
// In your AI Controller's OnPossess function
if (ADaAICharacter* AIChar = Cast<ADaAICharacter>(InPawn))
{
    if (UAIPerceptionComponent* PerceptionComp = AIChar->GetAIPerceptionComponent())
    {
        PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AYourAIController::OnPerceptionUpdated);
    }
}
```

#### Debugging Support

Add debug visualization in development builds:
```cpp
// In DaAICharacter constructor
#if WITH_EDITORONLY_DATA
    AIPerceptionComp->bEditableWhenInherited = true;
#endif
```

## Configuration Properties

### Sight Configuration Parameters

| Property | Description | Default Value | Notes |
|----------|-------------|---------------|--------|
| SightRadius | Maximum sight distance | 3000.0f | Adjust based on level scale |
| LoseSightRadius | Distance to lose sight | 3500.0f | Should be > SightRadius |
| PeripheralVisionAngleDegrees | FOV half-angle | 90.0f | 180° total FOV |
| MaxAge | Stimulus memory duration | 5.0f | How long to remember seen actors |

### Additional Senses (Future Enhancement)

The migration opens possibilities for additional senses:
- **Hearing**: `UAISenseConfig_Hearing`
- **Damage**: `UAISenseConfig_Damage`
- **Touch**: `UAISenseConfig_Touch`
- **Team**: `UAISenseConfig_Team`

## Testing the Migration

### 1. Compile and Fix Errors
- Ensure all references to `PawnSensingComp` are updated
- Check that new includes are added
- Verify function signatures match

### 2. In-Editor Testing
- Place AI characters in level
- Enable AI debug visualization: ` (apostrophe key) → Perception
- Verify sight cones appear correctly
- Test player detection at various distances

### 3. Runtime Verification
- Confirm AI detects player when in sight
- Verify "player seen" widget appears
- Test target switching on damage
- Check blackboard key updates

### 4. Performance Testing
- Monitor performance with multiple AI characters
- Compare with old PawnSensingComponent performance
- Optimize sight radius if needed

## Benefits of Migration

### 1. **Feature Rich**
- Multiple sense types in one component
- Built-in team affiliation support
- Stimulus aging and memory system

### 2. **Better Performance**
- Optimized spatial queries
- Configurable update rates
- Sense-specific optimizations

### 3. **Enhanced Debugging**
- Visual debug displays in editor
- Detailed perception logging
- Real-time perception state inspection

### 4. **Future Proof**
- Active development and support
- Integration with newer AI features
- Consistent with UE5 AI systems

### 5. **Gameplay Improvements**
- More realistic perception behavior
- Configurable per-sense parameters
- Better multiplayer support

## Common Issues and Solutions

### Issue 1: AI Not Detecting Targets
**Solution**: Ensure the target has a collision component and is set to generate overlap events.

### Issue 2: Detection Range Too Short/Long
**Solution**: Adjust `SightRadius` and `LoseSightRadius` in the sight configuration.

### Issue 3: Widget Not Appearing
**Solution**: Verify `MulticastOnPawnSeen` is still called in the perception update handler.

### Issue 4: Performance Impact
**Solution**: Reduce `PeripheralVisionAngleDegrees` or increase update intervals for distant AI.

## Code Cleanup Opportunities

After migration, consider:
1. Removing unused PawnSensingComponent references
2. Exposing perception parameters to blueprints
3. Adding editor-only visualization helpers
4. Creating reusable perception presets

## Timeline

1. **Phase 1**: Basic migration (lines changed: ~20)
2. **Phase 2**: Testing and validation
3. **Phase 3**: Performance optimization
4. **Phase 4**: Additional sense implementation (optional)

## Blueprint Usage Guide

### For Blueprint-Only AI Characters

If you're working with Blueprint-based AI characters, here's how to implement the new perception system:

#### 1. Adding AI Perception Component in Blueprint

1. **Open your AI Character Blueprint**
2. **Add Component**: Click "Add Component" → Search for "AI Perception"
3. **Select**: "AI Perception Component"
4. **Configure in Details Panel**:
   - Dominant Sense: `AI Sight`
   - Configure Senses → Add "AI Sight config"

#### 2. Configuring Sight Sense in Blueprint

In the AI Perception Component details:
1. **Senses Config** → Add Element → "AI Sight Config"
2. **Expand the Sight Config** and set:
   - **Sight Radius**: 3000.0 (detection range)
   - **Lose Sight Radius**: 3500.0 (slightly larger than sight radius)
   - **Peripheral Vision Angle Degrees**: 90.0 (180° total FOV)
   - **Max Age**: 5.0 (seconds to remember targets)
   - **Detection by Affiliation**:
     - ✓ Detect Enemies
     - ✓ Detect Neutrals  
     - ✓ Detect Friendlies

#### 3. Setting Up Perception Events

In the Event Graph:

1. **Add Event**: Right-click → "AI Perception" → "On Target Perception Updated"
2. **Connect the Event** to your logic:

```
Event On Target Perception Updated (AIPerception)
    ├─→ Cast To Pawn (Actor)
    │      ├─→ Successfully Sensed? (Branch)
    │      │      ├─→ True: Set Target Actor
    │      │      └─→ False: Clear Target Actor
    │      └─→ Cast Failed: Do Nothing
    └─→ Update UI Widget (if needed)
```

#### 4. Blueprint Node Reference

**Key Nodes for AI Perception:**
- `On Target Perception Updated` - Main perception event
- `Get Currently Perceived Actors` - Get all sensed actors
- `Get Actors Perception` - Get perception info for specific actor
- `Was Successfully Sensed` - Check if stimulus was positive
- `Get Stimulus Location` - Where the stimulus occurred
- `Get Stimulus Receiver Location` - Where AI was when sensing

#### 5. Common Blueprint Patterns

**Pattern 1: Simple Target Detection**
```
On Target Perception Updated
├─→ Was Successfully Sensed? (Branch)
│    ├─→ True: Set Blackboard Value as Object (TargetActor)
│    └─→ False: Clear Blackboard Value (TargetActor)
```

**Pattern 2: Filter by Actor Type**
```
On Target Perception Updated
├─→ Cast to Character
│    ├─→ Success: Get Player Controller
│    │    ├─→ Is Valid? → Set as Target
│    │    └─→ Not Valid → Ignore
│    └─→ Failed: Ignore
```

**Pattern 3: Multi-Sense Detection**
```
On Target Perception Updated
├─→ Break AIStimulus
│    ├─→ Switch on Sense Type
│    │    ├─→ Sight: Handle Visual Detection
│    │    ├─→ Hearing: Handle Sound Detection
│    │    └─→ Damage: Handle Damage Source
```

### For C++ Developers Using Blueprints

If you've implemented the C++ changes but want to expose more to Blueprint:

#### 1. Expose Perception Properties

Add to your header file:
```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Perception")
float SightRadius = 3000.0f;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Perception")
float PeripheralVisionAngle = 90.0f;

UFUNCTION(BlueprintCallable, Category="AI|Perception")
void UpdatePerceptionConfiguration();
```

#### 2. Blueprint-Callable Helper Functions

```cpp
UFUNCTION(BlueprintCallable, Category="AI|Perception")
TArray<AActor*> GetPerceivedEnemies() const;

UFUNCTION(BlueprintCallable, Category="AI|Perception")
bool CanSeeActor(AActor* Actor) const;

UFUNCTION(BlueprintImplementableEvent, Category="AI|Perception")
void OnEnemySpotted(AActor* Enemy);
```

### Debugging in Blueprint

#### 1. Visual Debugging
- **During Play**: Press ` (apostrophe) → Enable "Perception" debugging
- **See**: Sight cones, perception spheres, and detected actors

#### 2. Print String Debugging
Add these nodes for debugging:
```
On Target Perception Updated
├─→ Print String: "Perceived: [Actor Name]"
├─→ Print String: "Sensed: [Was Successfully Sensed]"
└─→ Print String: "Location: [Stimulus Location]"
```

#### 3. Common Blueprint Issues

**Issue**: AI not detecting anything
- **Check**: Is the target's collision set to "Pawn"?
- **Check**: Does target have "Auto Register as Pawn" enabled?

**Issue**: Detection range seems wrong
- **Check**: Sight Radius value in component details
- **Check**: Is AI character scaled? (affects perception radius)

**Issue**: AI instantly forgets targets
- **Check**: Max Age setting (should be > 0)
- **Check**: Lose Sight Radius (should be > Sight Radius)

### Migration Checklist for Blueprint Users

- [ ] Remove old Pawn Sensing Component from Blueprint
- [ ] Add AI Perception Component
- [ ] Configure Sight sense with appropriate values
- [ ] Replace "On See Pawn" with "On Target Perception Updated"
- [ ] Update any references to PawnSensingComp in Blueprint
- [ ] Test sight detection in editor with perception debugging
- [ ] Verify blackboard keys update correctly
- [ ] Check UI feedback still works (player spotted widget)

### Advanced Blueprint Features

#### Adding Multiple Senses

1. **In AI Perception Component**:
   - Senses Config → Add multiple configs
   - Each sense can have different parameters

2. **Example Multi-Sense Setup**:
   - **Sight**: 3000 unit radius, 90° FOV
   - **Hearing**: 1000 unit radius
   - **Damage**: Instant detection of damage source

#### Team Affiliation Setup

1. **Implement** `IGenericTeamAgentInterface` in C++ or
2. **Use** Team ID settings in AI Controller
3. **Configure** Detection by Affiliation per sense

#### Performance Optimization in Blueprints

1. **Limit Update Frequency**:
   - Use timers to throttle perception processing
   - Don't process every perception update

2. **Filter Stimuli**:
   - Only process relevant actor types
   - Ignore stimuli beyond certain distances

3. **LOD System**:
   - Disable perception on distant AI
   - Reduce perception frequency based on distance to player

## References

- [UE5 AI Perception Documentation](https://docs.unrealengine.com/5.5/en-US/ai-perception-in-unreal-engine/)
- [AI Debugging Tools](https://docs.unrealengine.com/5.5/en-US/ai-debugging-tools-in-unreal-engine/)
- [Perception System Overview](https://docs.unrealengine.com/5.5/en-US/perception-system-overview/)
- [Blueprint AI Tutorial](https://docs.unrealengine.com/5.5/en-US/blueprint-ai-in-unreal-engine/)
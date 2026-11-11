# Перенос сплит-скрина и геймпадов из UE 5.5 в UE 5.6

## Что подготовлено и проверено

Исходный проект DronesSetup содержит C++-модуль и Blueprint-ресурсы.
В UE 5.5 проверены две карты, назначение геймпадов игрокам, управление стиком
и быстрое гашение инерции. На этом компьютере установлен только UE 5.5:
сборка и запуск в UE 5.6 пока не проверялись.

## Вариант 1: новый проект на основе всего текущего

Если другого готового проекта ещё нет, проще сделать копию всего проекта.

1. Сохраните проект и закройте редактор. Оставьте исходную копию UE 5.5.
2. Запустите UE 5.6, выберите DronesSetup.uproject и Open a Copy.
3. Для C++ потребуется пересборка под 5.6: старые DLL от 5.5 не подходят.
4. Если автоматическая сборка не прошла, сгенерируйте Visual Studio project
   files для копии и соберите DronesSetupEditor / Development Editor / Win64.
   Target.cs можно оставить с IncludeOrderVersion.Unreal5_5 для первого
   совместимого запуска; переход на Unreal5_6 выполняйте отдельным шагом.
5. Откройте обе карты, Compile/Save Blueprint, проверьте Play Standalone.

Этот путь сохраняет имя модуля DronesSetup, все ссылки, настройки и карту.
После сохранения копии в 5.6 исходную версию 5.5 используйте отдельно.
Официальный порядок Open a Copy:
https://dev.epicgames.com/documentation/en-us/unreal-engine/updating-projects-to-newer-versions-of-unreal-engine?application_version=5.6

## Вариант 2: перенос в уже существующий другой проект UE 5.6

Далее предполагается, что целевой проект называется MyGame. Его настоящее
имя заменяет MyGame в примерах; имя переносимого модуля оставляем DronesSetup.

### 1. Подготовьте целевой проект

Создайте копию/коммит целевого проекта. В нём включите плагины:
- Enhanced Input;
- Raw Input;
- Chaos Vehicles (в .uproject: ChaosVehiclesPlugin).

Если это Blueprint-only проект, добавьте через редактор пустой C++-класс,
чтобы появились его основной модуль и Game/Editor Target.cs.
Закройте редактор перед копированием исходников и правкой конфигурации.

### 2. Перенесите C++-модуль

Скопируйте только папку Source/DronesSetup целиком в Source целевого проекта.
В ней должны быть DronesSetup.Build.cs и все .h/.cpp, в том числе:
- DronesSetup.cpp — регистрация внутренних клавиш Sony при запуске модуля;
- LocalMultiplayerSubsystem.* — второй игрок, автомобили, камеры, HUD;
- LocalMultiplayerViewportClient.* — присоединение и маршрутизация геймпадов;
- StickHalfAxisModifier.* — разделение оси стика на газ и тормоз;
- ArcadeCoastingComponent.* и ArcadeDrivingSettings.h — аркадная остановка.

У целевого проекта уже есть основной модуль. Поэтому в скопированном
Source/DronesSetup/DronesSetup.cpp замените только последнюю регистрацию:

    IMPLEMENT_PRIMARY_GAME_MODULE(FDronesSetupModule, DronesSetup, "DronesSetup");

на:

    IMPLEMENT_MODULE(FDronesSetupModule, DronesSetup);

Остальной StartupModule сохраните: он регистрирует DS4Raw_*.
В .uproject целевого проекта добавьте в массив Modules ещё один элемент,
сохранив его существующий основной модуль:

    { "Name": "DronesSetup", "Type": "Runtime", "LoadingPhase": "Default" }

В конструкторы существующих MyGame.Target.cs и MyGameEditor.Target.cs добавьте:

    ExtraModuleNames.Add("DronesSetup");

Исходные DronesSetup.Target.cs и DronesSetupEditor.Target.cs из корня Source
в другой проект переносить не нужно: используйте его собственные targets.
DronesSetup.Build.cs переносится целиком с зависимостями EnhancedInput,
InputCore, ApplicationCore, UMG, ChaosVehicles и DeveloperSettings.

Сохранение имени модуля DronesSetup важно: IMC_Default содержит сериализованные
ссылки на класс /Script/DronesSetup.StickHalfAxisModifier, а конфигурация —
на LocalMultiplayerViewportClient и ArcadeDrivingSettings. Если модуль переименовать,
нужно также изменить API-макросы, пути /Script и настроить Core Redirects.
Для первого переноса оставьте имя без изменений.

Сгенерируйте файлы решения и соберите целевой MyGameEditor под UE 5.6.
Binaries, Intermediate, Saved и DerivedDataCache из исходного проекта не копируйте.

### 3. Перенесите ресурсы через Migrate

В исходном UE 5.5 выберите всю папку Content/VehicleTemplate в Content Browser:
правой кнопкой → Migrate / Asset Actions → Migrate. Сохраните все зависимости
и укажите именно папку Content целевого проекта UE 5.6.
Если меню не предлагает Migrate для папки, выделите её ресурсы рекурсивно.

Нужны Input/IMC_Default, Input/Actions, Blueprint VehiclePlayerController,
VehicleUI, VehicleAdvPawn, GameMode и классы автомобилей. Материалы, меши,
колёса и прочие зависимости автомобилей должны попасть в Asset Report.
Перенос всей папки также захватывает тестовые карты.

Сохраняйте структуру /Game/VehicleTemplate/...: в C++ текущей реализации
жёстко указаны пути к IMC_Default, VehicleUI и используемым Input Actions.
Если их переместить, исправьте LoadObject/LoadClass в LocalMultiplayerSubsystem.cpp
и повторите сборку. Одних redirectors редактора для упакованной игры недостаточно.

Migrate переносит ресурсы и их зависимости. Наши C++-исходники и настройки
Config перенесите отдельно по этой инструкции.
Официальная процедура:
https://dev.epicgames.com/documentation/en-us/unreal-engine/migrating-assets-in-unreal-engine?application_version=5.6

### 4. Объедините настройки, сохранив настройки целевого проекта

В Config/DefaultEngine.ini добавьте/обновите параметры соответствующих секций:

    [/Script/EngineSettings.GameMapsSettings]
    bUseSplitscreen=True
    TwoPlayerSplitscreenLayout=Vertical
    bOffsetPlayerGamepadIds=False

    [/Script/Engine.Engine]
    GameViewportClientClassName=/Script/DronesSetup.LocalMultiplayerViewportClient

Vertical — игроки слева/справа; Horizontal — сверху/снизу.
Не заменяйте целиком его DefaultEngine.ini: карты, рендеринг и прочие параметры
целевого проекта должны остаться его собственными.
Если у него уже есть свой GameViewportClient, нужно объединить реализации
или сделать его наследником нашего класса: две независимые viewport-настройки
одновременно назначить нельзя.

В Config/DefaultInput.ini объедините:

    [/Script/Engine.InputSettings]
    DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput
    DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent
    bFilterInputByPlatformUser=False

Из исходного Config/DefaultInput.ini скопируйте всю секцию
[/Script/RawInput.RawInputSettings] с DeviceConfigurations для Sony 054C:09CC.
Не теряйте длинную строку AxisProperties/ButtonProperties: это профиль PS4.
Если такая секция уже есть, добавьте недостающую конфигурацию устройства;
не дублируйте один и тот же профиль.

В Config/DefaultGame.ini добавьте:

    [/Script/DronesSetup.ArcadeDrivingSettings]
    bEnableArcadeCoasting=True
    CoastDecayRate=10.0
    StopSpeed=5.0

### 5. Настройте GameMode и автомобили

На карте целевого проекта: World Settings → GameMode Override → перенесённый
VehicleAdvGameMode (SportsCar) или OffroadGameMode.
Либо в собственном GameMode задайте Default Pawn Class и Player Controller Class
по перенесённому шаблону. VehiclePlayerController уже исправлен: он создаёт
интерфейс и ввод на On Possess, после привязки игрока к автомобилю.

Добавьте PlayerStart на пригодное для автомобилей место. Вторую стартовую
позицию рядом создаёт наш subsystem. Для тесных карт подстройте логику спавна
в SetupPlayers. Также убедитесь, что нет другой логики Create Local Player,
которая дополнительно создаёт игроков, и нет Auto Possess Player 0 у всех
заранее размещённых автомобилей.

Если используете другой автомобиль, ему нужны обработчики действий IMC_Default
и собственная камера. Если позже откажетесь от Chaos Vehicles, сплит-скрин и
назначение геймпадов можно сохранить, а движение и ArcadeCoastingComponent
понадобится адаптировать под новую физику. Сейчас компонент рассчитан на
ChaosWheeledVehicleMovementComponent и автоматически добавляется двум
начальным автомобилям в SetupPlayers.

### 6. Проверьте редактор и отдельный запуск

1. Перезапустите редактор после изменения модулей и Config.
2. У IMC_Default должны отображаться Stick Half Axis, Dead Zone и Exponent;
   отсутствие класса означает, что C++-модуль не загрузился.
3. В Play установите Number of Players = 1, Net Mode = Play Standalone.
4. Убедитесь, что появились две половины и два отдельных автомобиля.
5. Присоедините геймпады кнопкой A / крестик в нужном порядке; F2 — сброс.
6. Проверьте газ/задний ход стиком, диагональ, кнопки и быстрое замедление
   после отпускания. Повторите с Xbox первым и PS4 первым.
7. Проверьте Standalone Game и собранный Development-пакет.
   В Packaging включите нужную карту. Из-за строковых LoadObject-ссылок
   добавьте /Game/VehicleTemplate/Input и /Game/VehicleTemplate/Blueprints
   в Additional Asset Directories to Cook, если ваша логика/карта не содержит
   явных ссылок на эти ресурсы.

Автоматический Scripts/Test-SplitScreen.ps1 можно перенести как основу теста,
но в новом проекте поменяйте имя .uproject, путь EngineRoot на UE_5.6 и,
если используете другие карты, имена карт. Его текущие ожидания по HUD и
автомобилям рассчитаны на перенесённый VehicleTemplate.

## Настройка небольшой инерции

Edit → Project Settings → Game → Arcade Driving.
- Enable Arcade Coasting: включает быстрое гашение качения.
- Coast Decay Rate = 10: скорость уменьшения горизонтальной скорости.
  6–8 дают больше остаточного качения; 12–15 — более резкую остановку.
- Stop Speed = 5 cm/s: остаточная скорость ниже порога обнуляется.

Те же значения находятся в Config/DefaultGame.ini. Помощь работает, когда
газ и тормоз/задний ход отпущены и хотя бы одно колесо касается земли.
Она не подаёт тормоз/задний ход, не гасит вертикальную скорость и выключена
в воздухе. Это первый аркадный шаг: разгон, сцепление, подвеска и заносы
по-прежнему определяются настройками Chaos Vehicle.

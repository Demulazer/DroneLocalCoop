# Настройки сплит-скрина и управления

## Где менять разделение экрана

В Unreal: Edit → Project Settings → Maps & Modes → Local Multiplayer.
Use Splitscreen включает разделение; Two Player Splitscreen Layout выбирает
Vertical (слева/справа) или Horizontal (сверху/снизу).
Те же параметры хранятся в Config/DefaultEngine.ini, в секции
[/Script/EngineSettings.GameMapsSettings]. Сейчас выбран Vertical.

Само включение Use Splitscreen не создаёт второго игрока. Его создаёт
Source/DronesSetup/LocalMultiplayerSubsystem.cpp. Там задаются количество
игроков, дополнительная стартовая позиция и настройка отдельных камер/HUD.
Автомобиль выбирается через GameMode карты: World Settings → GameMode Override,
а в соответствующем GameMode — Default Pawn Class.

## Где менять кнопки в редакторе

Откройте Content Browser → Content/VehicleTemplate/Input → IMC_Default.
Это основной Input Mapping Context: таблица «действие → клавиша/ось».
Внутри Mappings раскройте действие, затем конкретную привязку.
Поле Key меняет кнопку; плюс добавляет ещё одну кнопку к тому же действию.
Клавиатурные и геймпадные привязки находятся в одном контексте.

Основные действия лежат в Content/VehicleTemplate/Input/Actions:

| Действие | Назначение |
|---|---|
| IA_Throttle | Газ |
| IA_Brake | Тормоз и задний ход |
| IA_Steering | Руль |
| IA_Handbrake | Ручной тормоз |
| IA_LookAround | Обзор |
| IA_ToggleCamera | Смена камеры |
| IA_Reset | Восстановление положения автомобиля |

Используйте IA_Brake: в проекте также есть похожий по названию IA_Break,
но текущая схема тормоза использует IA_Brake.

Названия Gamepad в IMC_Default общие для Xbox и настроенного PS4.
Например, Gamepad Face Button Bottom — A на Xbox и крестик на PS4;
Gamepad Left Shoulder — LB / L1; Right Trigger Axis — RT / R2.
Оси выбирайте как аналоговые (Axis), если нужна плавная величина нажатия.
Не добавляйте DS4Raw_* в IMC_Default: это внутренние входы профиля Sony,
которые перед таблицей управления преобразуются в стандартные Gamepad_*.

Одинаковые привязки IMC_Default применяются к обоим игрокам.

## Движение левым стиком

В IMC_Default добавлены две привязки Gamepad Left Thumbstick Y:

| Действие | Modifier → Stick Half Axis → Negative Direction |
|---|---|
| IA_Throttle | Выключено: вверх даёт положительный газ |
| IA_Brake | Включено: вниз даёт положительный тормоз |

Gamepad Left Thumbstick X у IA_Steering отвечает за поворот.
Таким образом, диагональ стика одновременно задаёт газ/задний ход и руль.
Наклон регулирует силу ввода, а не только включает/выключает движение.
Триггеры газа и тормоза оставлены как дополнительные способы управления.
У IA_Brake удалены однократные Triggers Pressed/Released: теперь действие
обновляет аналоговое значение при удержании и сбрасывается при отпускании.

Чтобы настроить отклик газа, раскройте соответствующую привязку Left Thumbstick Y,
затем Modifiers → Stick Half Axis:

- Dead Zone: сейчас 0.15. Центральные 15% хода игнорируются; дальше значение
  плавно увеличивается до 1. При дрейфе стика увеличьте, например, до 0.20.
- Exponent: сейчас 1.0, линейный отклик. Например, 2.0 даёт более мягкую реакцию
  около центра; полный наклон по-прежнему даёт полный газ.
- Negative Direction: включено только у тормоза/заднего хода.

Настройте обе привязки Y при изменении поведения вперёд и назад.
Для руля у привязки Left Thumbstick X можно добавить стандартные Modifiers:
Dead Zone (центральная мёртвая зона) и Response Curve Exponential (кривая отклика).
Negate инвертирует направление оси, Scalar меняет величину ввода.
Не добавляйте вторую Dead Zone к Y без необходимости: Stick Half Axis уже её применяет.

Движение остаётся автомобильным: при наклоне назад движущийся вперёд автомобиль
сначала тормозит, затем включается задний ход, как при S в шаблоне.
За это отвечает Reverse As Brake в Vehicle Movement Component автомобиля.
Возврат стика в центр убирает газ/тормоз и быстро гасит качение через Arcade Driving.
Это не режим мгновенного перемещения персонажа.

## Как добавить новое действие

Пример: назначить действие на Y у Xbox / треугольник у PS4.

1. В папке Input/Actions создайте Input → Input Action, например IA_Horn.
2. Выберите Value Type = Digital (Bool) для кнопки. Для одной аналоговой
   величины используйте Axis1D, для двух осей — Axis2D.
3. Откройте IMC_Default, добавьте Mapping с действием IA_Horn.
4. В Key выберите Gamepad Face Button Top. При необходимости добавьте
   вторую привязку клавиатуры к этому же действию.
5. Откройте Content/VehicleTemplate/Blueprints/VehicleAdvPawn → Event Graph.
6. Добавьте событие IA_Horn. Подключите Started к однократному действию,
   Triggered — к действию, которое должно выполняться при удержании,
   Completed/Canceled — к остановке/сбросу при отпускании.
7. Compile и Save. Сохраните также IMC_Default и новый Input Action.

Одна привязка только создаёт сигнал: без обработчика события новое игровое
действие не появится. VehicleAdvPawn — общий Blueprint управления автомобилями;
специфичное поведение можно добавить в SportsCar_Pawn или OffroadCar_Pawn.
Не используйте Get Player Controller 0 для действий обоих игроков: обращайтесь
к Self / контроллеру текущего автомобиля, чтобы воздействовать на своего игрока.

## Что находится в C++ и Raw Input

Кнопки A / крестик для присоединения и F2 для сброса назначений сейчас заданы
в Source/DronesSetup/LocalMultiplayerViewportClient.cpp, в InputKey.
Эти специальные команды меняются отдельно от IMC_Default и требуют пересборки.

Config/DefaultInput.ini содержит профиль проводного PS4 Sony 054C:09CC:
он описывает соответствие физических осей/кнопок устройства стандартным входам.
Обычное игровое переназначение выполняйте в IMC_Default, а не в этом профиле.
Для новой модели устройства профиль проверяется отдельно.
Project Settings → Input содержит также общие параметры захвата мыши и классов
Enhanced Input. Старые списки Action/Axis Mappings не являются основной таблицей
управления этого проекта.

После обычной правки IMC_Default/Blueprint достаточно Compile/Save и нового Play.
После изменения профиля устройства, GameViewportClient или C++ перезапустите
редактор; изменения C++ сначала требуют сборки.

## Небольшая инерция

Project Settings → Game → Arcade Driving: Coast Decay Rate = 10, Stop Speed = 5 cm/s.
Больше Coast Decay Rate — быстрее остановка; меньше — больше качение.
Те же настройки: Config/DefaultGame.ini, секция DronesSetup.ArcadeDrivingSettings.
Помощь включается на земле при отпущенных газе и заднем ходе; в воздухе не действует.
Перенос в другой проект UE 5.6: MIGRATION_UE56.md.



## Подбор и бросок

IA_Pickup: F / Gamepad Face Button Bottom (A, крестик).
IA_Throw: R / Gamepad Face Button Left (X, квадрат).
Обработчики находятся в VehicleAdvPawn → Pickup and Throw.
Предметы — BP_ThrowableCube и его дочерние Blueprint. Подробнее: THROWABLES.md.

Чтобы новые действия не включали одновременно прежний газ/тормоз,
дополнительный газ с Face Button Left перенесён на Face Button Top (Y, треугольник),
а дополнительный тормоз с Face Button Bottom — на Face Button Right (B, круг).
Управление стиком и триггерами сохранено.

Прицеливание: удерживайте X / квадрат / R, чтобы видеть красную траекторию.
Бросок выполняется на отпускание (IA_Throw → Completed), отмена Canceled только убирает прицел.
Граф: VehicleAdvPawn → Throw Aiming. Прицел виден только своему игроку.

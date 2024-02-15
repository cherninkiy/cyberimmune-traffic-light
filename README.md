# Traffic lights control with KasperskyOS

## Описание задачи

### О примере

Функционально пример представляет из себя заготовку системы управления для светофора. Предлагается реализовать несколько сущностей, описанных в архитектуре системы, а также реализовать политики безопасности, которые обеспечат работу системы согласно политике архитектуры.

Цели и предположения безопасности обсуждаются на онлайн-курсе (ссылка будет добавлена позже). 

### Инструкция по настройке окружения для разработки

#### Настройка системы

Пошаговая видео-инструкция по развёртыванию KasperskyOS в виртуальной машине под управлением Oracle VirtualBox доступна в составе этого курса: https://stepik.org/course/73418

Использование KasperskyOS в docker контейнере описано на этой странице: https://support.kaspersky.ru/help/KCE/1.1/ru-RU/using_docker.htm

### Сборка и запуск примера

* с использованием Makefile:
  *  <b>сборка docker образа с KasperskyOS</b>. 
  В этом проекте в качестве базового образа используется Ubuntu 20.04, при желании можно поменять на Ubuntu 22.04 или Debian 10.12.
    
        ```make d-build```   

        <i>Примечание</i>: установочный deb файл с KasperskyOS Community Edition SDK должен быть скопирован в папку с Dockerfile (корневую папку проекта) перед запуском этой команды

        
  *  <b>запуск контейнера</b>

        ```make develop```

        Примечание: в контейнере предполагается работать не от имени root, а от пользователя user. 
        
        Если где-то это будет мешать, нужно в Makefile для цели develop убрать в команде запуска аргумент "--user user"

        

  * <b> сборка проекта</b>. Примечание: эта команда должна выполняться внутри контейнера

    ```make build``` 

  * <b> запуск в qemu</b>. Примечание: эта команда должна выполняться внутри контейнера

    ```make sim``` 

  *  <b> удаление временных файлов</b>. Сейчас удаляет папку build со всем содержимым. 

        ```make clean``` 
    
        Собранный образ с SDK можно удалить командой 

        ```docker rmi kos:1.1.1.40u20.04```

## Домашнее задание: День 2

1. ✅ В примере «светофор» реализовать сущность diagnostics по аналогии с echo2c.
Ожидаемый результат: после запуска системы появляется сообщение от новой
сущности.

Добавлена новая сущность diagnostics.

![Diagnostics Entity](docs/day2-hw/added_diagnostics_entity.png)

2. ✅ Необязательное задание1 повышенной сложности:
реализовать IPC взаимодействие между LightsGPIO и Diagnostics. Ожидаемый
результат: тестовое сообщение от LightsGPIO получено в Diagnostics
Примечание: для этого задания может быть полезным посмотреть код примера из
SDK secure_logger

Реализовано взаимодействие сущностей lights_gpio и diagnostics.

![Diagnostics Entity](docs/day2-hw/added_lights_to_diagnostics_connection.png)

3. ✅ Отправить все свои изменения в свой репозиторий на github в ветку [day2-homework-diagnostics-dummy](https://github.com/cherninkiy/cyberimmune-traffic-light/tree/day2-homework-diagnostics-dummy) для задания 1, если сделано и задание 2, то в ветку [day2-homework-diagnostics-functional](https://github.com/cherninkiy/cyberimmune-traffic-light/tree/day2-homework-diagnostics-functional)

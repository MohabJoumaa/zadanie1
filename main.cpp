#include <chrono>
#include <random>
#include <mutex>
#include <atomic>
#include <thread>
#include <string>
#include <locale>
#include <codecvt>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib> /*для std::system*/
#include "header.h"

/*Минимальное и максимальное время приостановки клиентов, мс*/
constexpr std::uint32_t minDelay{100}, maxDelay{10000};
constexpr std::uint32_t numberOfClients{5};

constexpr auto serverPeriod = std::chrono::milliseconds{1000};

std::uniform_int_distribution<std::uint32_t>
  delayDistribution{minDelay, maxDelay};
std::uniform_int_distribution<tid_t>
  requestDistribution{0, numberOfTasks - 1};
std::mutex mutexForRandom;

/*Класс runTime используется для вывода интервалов времени в формате с:мс:мкс*/
template <class ClockType> class runTime {
  typename ClockType::time_point zeroPoint;
public:
  runTime () = default;
  explicit runTime (typename ClockType::time_point start) : zeroPoint{start} {}
  typename ClockType::duration timeSinceStart () const {
    return ClockType::now() - zeroPoint;
  }
};

/*Это шаблонная функция перегрузки оператора вывода для класса runTime, который параметризуется типом ClockType.
Функция принимает ссылку на объект ostream (поток вывода) и ссылку на константный объект класса runTime.

Функция использует пространство имён std::chrono для работы с временными интервалами. Она вызывает метод
timeSinceStart() объекта rt, который возвращает временной интервал с момента запуска.

Функция выводит время в формате ЧЧ:ММ:ССС, где ЧЧ - количество секунд с момента запуска, ММ - количество 
миллисекунд, ССС - количество микросекунд. Для форматированного вывода используются функции setw() и count().

Функция возвращает ссылку на объект ostream, после вывода времени.*/
template <class ClockType>
std::ostream& operator<< (std::ostream& os,
                           runTime<ClockType> const& rt) {
  using namespace std::chrono;
  auto const d = rt.timeSinceStart();
  return os << std::setw(4) << (duration_cast<seconds>(d)).count() << ':'
     << std::setw(3) << (duration_cast<milliseconds>(d)).count()%1000 << ':'
     << std::setw(3) << (duration_cast<microseconds>(d)).count()%1000;
}

/*runTime<std::chrono::steady_clock> const* pRunTime = nullptr; - это объявление указателя pRunTime, 
указывающего на переменную типа runTime<std::chrono::steady_clock> const, которая хранит адрес объекта 
типа runTime<std::chrono::steady_clock>. Изначально указатель установлен в значение nullptr.
std::ofstream logFile{}; - это объявление переменной logFile, выступающей в качестве объекта потока вывода 
(ofstream). Он используется для записи данных в файл. Изначально он инициализирован пустыми фигурными скобками,
что приведет к созданию пустого файла.
std::atomic<bool> quitFlag{false}; - это объявление переменной quitFlag,
выступающей в качестве атомарного объекта типа bool. Атомарные типы обеспечивают простое и безопасное 
обновление этой переменной несколькими потоками одновременно. Значение переменной инициализировано false (ложь).
std::atomic<std::uint32_t> threadCounter; - это объявление переменной threadCounter, выступающей в качестве
атомарного объекта типа std::uint32_t (беззнаковое целое число). Значение переменной не инициализировано и 
будет зависеть от типа возвращаемого значения переменной.*/
runTime<std::chrono::steady_clock> const* pRunTime = nullptr;

std::ofstream logFile{};
std::atomic<bool> quitFlag{false};
std::atomic<std::uint32_t> threadCounter;

/*Этот код определяет функцию serverImpl(), которая будет выполняться в потоке.В функции сначала записывается
 сообщение в logFile о запуске сервера, используя значение переменной pRunTime. Затем запускается бесконечный
 цикл, который будет выполняться до тех пор, пока значение переменной quitFlag не станет true. В каждой
 итерации цикла поток "засыпает" на промежуток времени, заданный переменной serverPeriod, с помощью функции
 std::this_thread::sleep_for(). Затем вызывается функция runTaskManager().После выхода из цикла записывается
 сообщение в logFile о завершении работы сервера. Затем значение переменной threadCounter уменьшается на
 единицу.*/
void serverImpl (void) {
  logFile << *pRunTime << " | Сервер | запуск" << std::endl;
  while (!quitFlag) {
    std::this_thread::sleep_for(serverPeriod);
    runTaskManager();
  }
  logFile << *pRunTime << " | Сервер | завершение" << std::endl;
  --threadCounter;
}
/*В данном коде определена функция clientImpl, которая принимает два аргумента - clientId типа std::uint32_t
и engine типа std::minstd_rand&.
Внутри функции происходит следующее:
Создается переменная delay типа decltype(engine)::result_type и переменная taskId 
типа decltype(engine)::result_type. decltype(engine) используется для получения типа,
возвращаемого объектом engine.
В лог-файл записывается время запуска клиента с указанным clientId.
В бесконечном цикле:
Захватывается mutexForRandom для безопасного доступа к генератору случайных чисел engine.
Генерируется случайное время задержки delay и случайный идентификатор задачи taskId.
Mutex разблокируется.
Поток засыпает на время delay.
Если флаг quitFlag установлен, цикл прерывается.
Выполняется запрос задачи с указанным taskId.
В лог-файл записывается время, идентификатор клиента и идентификатор задачи.
В лог-файл записывается время, идентификатор клиента и слово "завершение".
Уменьшается счетчик потоков threadCounter.*/
void clientImpl (std::uint32_t clientId, std::minstd_rand& engine) {
  std::remove_reference<decltype(engine)>::type::result_type delay, taskId;
  logFile << *pRunTime << " | Клиент" << clientId << " | запуск" << std::endl;
  while (true) {
    mutexForRandom.lock();
    delay = delayDistribution(engine);
    taskId = requestDistribution(engine);
    mutexForRandom.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds{delay});
    if (quitFlag)
      break;

    requestTask(taskId);
    logFile << *pRunTime << " | Клиент" << clientId
            << " | " << taskId << std::endl;
  }
  logFile << *pRunTime << " | Клиент" << clientId
          << " | завершение" << std::endl;
  --threadCounter;
}

/*В этом коде определена функция с именем "task", которая принимает один аргумент типа "tid_t" 
с именем "taskId".Внутри функции выполняются следующие действия :

Записывается значение переменной "*pRunTime" в файл "logFile".Значение переменной "*pRunTime" вероятно
является указателем на некоторое время выполнения задачи.
Затем записывается строка " | Сервер | " в файл "logFile".
Затем записывается значение переменной "taskId" в файл "logFile".
Наконец, записывается символ новой строки в файл "logFile", чтобы следующая запись началась с новой строки.
Переменная "logFile" скорее всего является объектом класса "ofstream", связанным с файлом.Она должна быть
предварительно открыта в режиме записи до вызова этой функции.*/
void task (tid_t taskId) {
  logFile << *pRunTime << " | Сервер | " << taskId << std::endl;
}

/*Сначала устанавливается локаль на русскую с помощью функции std::setlocale(LC_ALL, "RU").
Далее создается локаль для кодирования UTF-8 с помощью класса std::codecvt_utf8<wchar_t>, который 
будет использоваться для правильной локализации вывода в лог-файл.
Затем создается объект logFile, который проимбевается созданной локалью с помощью функции imbue().
Создается объект rt, который представляет текущее время запуска программы с использованием 
std::chrono::steady_clock.
Затем генерируется случайное число seed с использованием текущего времени в качестве семени для генератора 
случайных чисел engine, созданного с помощью std::minstd_rand.
Устанавливается имя лог-файла logName и открывается файл для записи.
Запускается сервер в отдельном потоке с помощью std::thread.
Создаются numberOfClients клиентов в отдельных потоках с помощью std::thread.
Устанавливается счетчик потоков threadCounter для контроля количества работающих потоков.
Выводится соответствующее сообщение на экран с помощью std::wcout.
С помощью std::system("pause") программа ожидает нажатия любой клавиши.
Устанавливается флаг завершения работы quitFlag в true.
Программа ожидает завершения всех потоков с помощью цикла while(threadCounter).*/
int main () {
  std::setlocale(LC_ALL, "RU");
  std::locale const utf8Locale{std::locale{}, new std::codecvt_utf8<wchar_t>{}};
  logFile.imbue(utf8Locale);

  runTime<std::chrono::steady_clock> const rt{std::chrono::steady_clock::now()};
  pRunTime = &rt;

  auto const seed = static_cast<std::minstd_rand::result_type>(
    std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now()
    )
  );
  std::minstd_rand engine{seed};

  std::string logName = "log.txt";
  logFile.open(logName);
  std::thread server{serverImpl};
  server.detach();
  std::thread clients[numberOfClients];
  for (std::uint32_t id = 0; id < numberOfClients; ++id) {
    clients[id] = std::thread{clientImpl, id, std::ref(engine)};
    clients[id].detach();
  }
  threadCounter = 1 + numberOfClients;
  std::wcout << L"Сервер и клиенты запущены.\n"
                L"Для завершения их работы нажмите любую клавишу.\n"
             << std::endl;
  std::system("pause");

  quitFlag = true;
  while(threadCounter);

  /*Сервер и все клиенты завершили выполнение*/
  /*Лог-файл закрывается с помощью функции close().
Выводится текст "Готово." с использованием потока wcout.
Выполняется команда system("pause"), которая останавливает выполнение программы до тех пор, 
пока пользователь не нажмет любую клавишу.
Возвращается значение 0, что обозначает успешное завершение программы.*/
  logFile.close();
  std::wcout << L"Готово." << std::endl;
  std::system("pause");
  return 0;
}



/*функции serverImpl и clientImpl выполняют задачи сервера и клиента соответственно.

Функция serverImpl выполняет обработку задач на сервере. Она запускается в отдельном потоке и работает 
пока флаг quitFlag равен false. Внутри функции происходит задержка на время serverPeriod, после которой 
вызывается функция runTaskManager, которая отсутствует в данном фрагменте кода. Затем функция опять засыпает
на serverPeriod и процесс повторяется. При изменении флага quitFlag на true, функция завершает свою работу.

Функция clientImpl выполняет обработку задач на клиенте. Она также запускается в отдельном потоке и работает
бесконечно. Внутри цикла происходит выбор случайного значения задержки delay из диапазона minDelay и maxDelay,
а также случайного значения задачи taskId из диапазона от 0 до numberOfTasks - 1. Затем происходит задержка на
время delay, выполнение запроса requestTask с задачей taskId и запись соответствующей информации в лог-файл. 
Если флаг quitFlag становится true, цикл прерывается и функция завершает свою работу.

Функция task представляет собой функцию, которая выполняет определенную задачу на сервере. Она вызывается 
в функции serverImpl и также записывает информацию о выполнении задачи в лог-файл.

Функция main является точкой входа в программу. В ней происходит инициализация необходимых переменных и 
создание потоков для выполнения сервера и клиентов. После запуска потоков выводится информация о запуске 
программы и ожидается нажатие клавиши. При нажатии клавиши флаг quitFlag устанавливается в true, все потоки
завершают свою работу, лог-файл закрывается и программа завершается.*/
# GSMtrap
Проект - ловушка для зверушки для ленивого охотника на ардуине + GSM900.

Практически все время устройство находится в глубуком сне, просыпаясь раз в 8 секунд проверить состояние ловушки(закрыта/нет). Автономность максимальная, которую можно выжать из неспециализированной платы.

Алгоритм - отправили смс с коммандой addphone на устройство, номер отправителя записался в постоянную память, после устройство уснуло.

Прижатие пина 5 к земле заставляет держать модем включенным, дабы принять команду добавления номера.

Внесение нового номера затирает старый. 

При срабатывании ловушки(замыкание контактов) высокий сигнал поступает на пин 4, модем отправляет смску на номер, записанный в памяти. 
Не забудь резисторы для прижатия соответствующих пинов к + и -!

Кетайцы накосячили с разводкой платы, поэтому выключение модема происходит посредством AT команды, а включение управляется сигналом на пин 9.

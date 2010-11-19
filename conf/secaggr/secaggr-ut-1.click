
as :: AvailableSensors();

Sensor(ID 1, SENSORS as, VALUE 10, DEBUG false, SAMPLE 1)
 -> Unqueue()
 -> Sink(SENSORS as, DEBUG true)
 -> Discard();


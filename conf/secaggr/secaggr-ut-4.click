
as1 :: AvailableSensors();

s1 :: Sensor(ID 1, SENSORS as1, VALUE 10, DEBUG false, SAMPLE 1);
s2 :: Sensor(ID 2, SENSORS as1, VALUE 10, DEBUG false, SAMPLE 1);
s3 :: Sensor(ID 3, SENSORS as1, VALUE 10, DEBUG false, SAMPLE 1);

ap1 :: AggregationPoint(SENSORS as1);

s1 -> [0] ap1;
s2 -> [1] ap1;
s3 -> [2] ap1;

index1 :: AvailableSensorsIndex(as1);
hbh1 :: HBHAggregationPoint(INDEX index1);

ap1 -> hbh1;

as2 :: AvailableSensors(as1);

s4 :: Sensor(ID 4, SENSORS as2, VALUE 20, DEBUG false, SAMPLE 1);
s5 :: Sensor(ID 5, SENSORS as2, VALUE 20, DEBUG false, SAMPLE 1);
s6 :: Sensor(ID 6, SENSORS as2, VALUE 20, DEBUG false, SAMPLE 1);

ap2 :: AggregationPoint(SENSORS as2);

s4 -> [0] ap2;
s5 -> [1] ap2;
s6 -> [2] ap2;

index2 :: AvailableSensorsIndex(as2);
hbh2 :: HBHAggregationPoint(INDEX index2);

ap2 -> [0] hbh2;
hbh1 -> Unqueue() -> [1] hbh2;

hbh2
 -> Unqueue()
 -> Sink(SENSORS as2, DEBUG true)
 -> Discard();


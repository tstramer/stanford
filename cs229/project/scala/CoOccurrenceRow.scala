
final case class CoOccurrenceRow(
  entity1: String,
  entity2: String,
  count: Int
)

object CoOccurrenceRow {

  type Tuple = (String, String, Int)

  def fromTuple(tuple: Tuple): CoOccurrenceRow = {
    CoOccurrenceRow(tuple._1, tuple._2, tuple._3)
  }
}

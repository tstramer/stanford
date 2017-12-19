
final case class TweetRow(
  userID: Long,
  tweetID: Long,
  followerCount: Long,
  tweepCred: Double,
  entitiesStr: String,
  name: String = "",
  screenName: String = "",
  bio: String = "",
  tweetText: String = ""
) {
  val entities = entitiesStr.split(TweetRow.EntityDelimiter)
}

object TweetRow {
  val EntityDelimiter = ","

  type Tuple = (Long, Long, Long, Double, String, String, String, String, String)

  def fromTuple(tuple: Tuple): TweetRow = {
    TweetRow(tuple._1, tuple._2, tuple._3, tuple._4, tuple._5, tuple._6, tuple._7, tuple._8, tuple._9)
  }
}
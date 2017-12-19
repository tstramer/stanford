import com.twitter.scalding_internal.job.{TwitterJob, RequiredBinaryComparators, TwitterDateJob}
import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.common_internal.text.pipeline.TwitterPhraseExtractor
import com.twitter.common_internal.text.version.PenguinVersion
import com.twitter.pluck.source.StatusSource
import com.twitter.pluck.source.combined_user_source.MostRecentCombinedUserSnapshotSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import java.util.Locale
import scala.collection.JavaConverters._
import scala.util.Random

/*object KMeansClustering {


  def runSingleStep[Point](
    points: TypedPipe[Point],
    centroids: Iterable[Point],
    distance: (Point, Point) => Double,
    mean:
  ) = {
    val centroidsWithID = centroids.zipWithIndex

    points
      .map { point =>

        val distances: Map[Int, Double] =
          centroidsWithID.map { case (centroid, id) =>
            id -> distance(point, centroid)
          }.toMap
        val (closestCentroid, closestID) = centroidsWithID.minBy { case (centroid, id) =>
          distances(id)
        }

        (closestID, Seq(point))
      }
      .sumByKey
      .withReducers(centroids.size)
      .toTypedPipe
      .map { case (centroidID, points) =>
        val centroid = centroidsWithID(centroid)
        val
      }
  }
}*/

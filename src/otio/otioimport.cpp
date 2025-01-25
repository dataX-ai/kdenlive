/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioimport.h"

#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "otioutil.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/view/timelinewidget.h"

#include <KLocalizedString>

#include <QFileDialog>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/generatorReference.h>
#include <opentimelineio/imageSequenceReference.h>

OtioImport::OtioImport(QObject *parent)
    : QObject(parent)
{
}

void OtioImport::slotImport()
{
    // Get the file name.
    std::shared_ptr<OtioImportData> data = std::make_shared<OtioImportData>();
    data->otioFile = QFileInfo(QFileDialog::getOpenFileName(pCore->window(), i18n("OpenTimelineIO Import"), pCore->currentDoc()->projectDataFolder(),
                                                            QStringLiteral("%1 (*.otio)").arg(i18n("OpenTimelineIO Project"))));
    if (!data->otioFile.exists()) {
        // TODO: Error handling?
        return;
    }

    // Open the OTIO timeline.
    OTIO_NS::ErrorStatus otioError;
    data->otioTimeline = OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>(
        dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(data->otioFile.filePath().toStdString(), &otioError)));
    if (!data->otioTimeline || OTIO_NS::is_error(otioError)) {
        // TODO: Error handling?
        return;
    }

    // Try to find an existing profile that matches the frame rate
    // and resolution, otherwise create a custom profile.
    //
    // Note that OTIO files do not contain information about rendering,
    // so we get the resolution from the first video clip.
    const double otioFps = data->otioTimeline->duration().rate();
    QVector<QString> profileCandidates;
    auto &profileRepository = ProfileRepository::get();
    auto profiles = profileRepository->getAllProfiles();
    for (const auto &i : profiles) {
        const auto &profileModel = profileRepository->getProfile(i.second);
        if (profileModel->fps() == otioFps) {
            profileCandidates.push_back(i.second);
        }
    }
    QSize videoSize(1920, 1080);
    for (const auto &otioItem : data->otioTimeline->tracks()->children()) {
        if (auto otioTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(otioItem)) {
            if (OTIO_NS::Track::Kind::video == otioTrack->kind()) {
                for (const auto &otioClip : otioTrack->find_clips()) {
                    if (auto otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {
                        const QString file = resolveFile(QString::fromStdString(otioExternalReference->target_url()), data->otioFile);
                        videoSize = getVideoSize(file);
                        break;
                    }
                }
            }
        }
    }
    QString profile;
    if (!profileCandidates.empty()) {
        for (const auto &i : profileCandidates) {
            const auto &profileModel = profileRepository->getProfile(i);
            const bool progressive = profileModel->progressive();
            const int width = profileModel->width();
            const int height = profileModel->height();
            if (progressive && videoSize.width() == width && videoSize.height() == height) {
                profile = i;
                break;
            }
        }
    }
    if (profile.isEmpty()) {
        QDomDocument doc;
        QDomElement elem = doc.createElement("profile");
        elem.setAttribute("description", QString("%1x%2 %3FPS").arg(videoSize.width()).arg(videoSize.height()).arg(otioFps));
        bool adjusted = false;
        std::pair<int, int> fraction = KdenliveDoc::getFpsFraction(otioFps, &adjusted);
        elem.setAttribute("frame_rate_num", fraction.first);
        elem.setAttribute("frame_rate_den", fraction.second);
        elem.setAttribute("progressive", 1);
        elem.setAttribute("sample_aspect_num", 1.0);
        elem.setAttribute("sample_aspect_den", 1.0);
        elem.setAttribute("display_aspect_num", videoSize.width());
        elem.setAttribute("display_asepct_den", videoSize.height());
        elem.setAttribute("colorspace", 0);
        elem.setAttribute("width", videoSize.width());
        elem.setAttribute("height", videoSize.height());
        ProfileParam profileParam(elem);
        profile = ProfileRepository::get()->saveProfile(&profileParam);
    }

    // Create a new document.
    pCore->projectManager()->newFile(profile, false);
    data->timeline = pCore->currentDoc()->getTimeline(pCore->currentTimelineId());
    data->defaultTracks = data->timeline->getAllTracksIds();

    // Find all of the OTIO media references and add them to the bin. When
    // the bin clips are ready, import the timeline.
    //
    // TODO: Add a progress dialog?
    for (const auto &otioClip : data->otioTimeline->find_clips()) {
        if (auto otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {
            const QString file = resolveFile(QString::fromStdString(otioExternalReference->target_url()), data->otioFile);
            const auto i = data->otioExternalRefToBinId.find(file);
            if (i == data->otioExternalRefToBinId.end()) {
                Fun undo = []() { return true; };
                Fun redo = []() { return true; };
                ++data->waitingBinIds;
                std::function<void(const QString &)> callback = [this, data](const QString &) {
                    --data->waitingBinIds;
                    if (0 == data->waitingBinIds) {
                        importTimeline(data);
                    }
                };
                const QString binId = ClipCreator::createClipFromFile(file, pCore->projectItemModel()->getRootFolder()->clipId(), pCore->projectItemModel(),
                                                                      undo, redo, callback);
                data->otioExternalRefToBinId[file] = binId;

                // Get the start timecode from the media.
                const QString timecode = getTimecode(file);
                if (!timecode.isEmpty()) {
                    data->binIdToTimecode[binId] = timecode;
                }
            }
        }
    }
}

void OtioImport::importTimeline(const std::shared_ptr<OtioImportData> &data)
{
    // Import the tracks.
    auto otioChildren = data->otioTimeline->tracks()->children();
    for (auto i = otioChildren.rbegin(); i != otioChildren.rend(); ++i) {
        if (auto otioTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(*i)) {
            int trackId = 0;
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            data->timeline->requestTrackInsertion(-1, trackId, QString::fromStdString(otioTrack->name()), OTIO_NS::Track::Kind::audio == otioTrack->kind(),
                                                  undo, redo);
            importTrack(data, otioTrack, trackId);
        }
    }
    data->timeline->updateDuration();

    // Import the OTIO markers as guides.
    for (const auto &otioMarker : data->otioTimeline->tracks()->markers()) {
        const GenTime pos(otioMarker->marked_range().start_time().value(), data->otioTimeline->duration().rate());
        importMarker(otioMarker, pos, data->timeline->getGuideModel());
    }

    // Delete the default tracks.
    for (int id : data->defaultTracks) {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        data->timeline->requestTrackDeletion(id, undo, redo);
    }
}

void OtioImport::importTrack(const std::shared_ptr<OtioImportData> &data, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &otioTrack, int trackId)
{
    auto otioChildren = otioTrack->children();
    for (auto i = otioChildren.begin(); i != otioChildren.end(); ++i) {
        if (auto otioClip = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Clip>(*i)) {
            importClip(data, otioClip, trackId);
        }
    }
}

void OtioImport::importClip(const std::shared_ptr<OtioImportData> &data, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> &otioClip, int trackId)
{
    const auto otioTrimmedRange = otioClip->trimmed_range_in_parent();
    if (otioTrimmedRange.has_value()) {

        // Find the bin clip that corresponds to the OTIO media reference.
        QString binId;
        if (auto otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {
            const QString file = resolveFile(QString::fromStdString(otioExternalReference->target_url()), data->otioFile);
            const auto i = data->otioExternalRefToBinId.find(file);
            if (i != data->otioExternalRefToBinId.end()) {
                binId = i.value();
            }
        } else if (auto otioImagSequenceReference = dynamic_cast<OTIO_NS::ImageSequenceReference *>(otioClip->media_reference())) {
            // TODO: Image sequence references.
        } else if (auto otioGeneratorReference = dynamic_cast<OTIO_NS::GeneratorReference *>(otioClip->media_reference())) {
            // TODO: Generator references.
        }

        // Insert the clip into the track.
        if (!binId.isEmpty()) {
            const OTIO_NS::RationalTime otioTimelineDuration = data->otioTimeline->duration();
            const int position = otioTrimmedRange.value().start_time().rescaled_to(otioTimelineDuration).round().value();
            int clipId = -1;
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            data->timeline->requestClipInsertion(binId, trackId, position, clipId, false, true, true, undo, redo);
            if (clipId != -1) {

                // Resize the clip.
                const int duration = otioTrimmedRange.value().duration().rescaled_to(otioTimelineDuration).round().value();
                data->timeline->requestItemResize(clipId, duration, true, false, -1, true);

                // Slip the clip.
                const int start = otioClip->trimmed_range().start_time().rescaled_to(otioTimelineDuration).round().value();
                int slip = start;
                const auto i = data->binIdToTimecode.find(binId);
                if (i != data->binIdToTimecode.end()) {
                    const OTIO_NS::RationalTime otioTimecode = OTIO_NS::RationalTime::from_timecode(i->toStdString(), otioTimelineDuration.rate());
                    slip -= otioTimecode.value();
                }
                if (slip != 0) {
                    data->timeline->requestClipSlip(clipId, -slip, false, true);
                }

                // Add markers.
                if (std::shared_ptr<ClipModel> clipModel = data->timeline->getClipPtr(clipId)) {
                    for (const auto &otioMarker : otioClip->markers()) {
                        const OTIO_NS::RationalTime otioMarkerStart = otioMarker->marked_range().start_time().rescaled_to(otioTimelineDuration).round();
                        const GenTime pos(start + otioMarkerStart.value(), otioTimelineDuration.rate());
                        importMarker(otioMarker, pos, clipModel->getMarkerModel());
                    }
                }
            }
        }
    }
}

void OtioImport::importMarker(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Marker> &otioMarker, const GenTime &pos,
                              const std::shared_ptr<MarkerListModel> &markerModel)
{
    // Check the OTIO metadata for the marker type, otherwise find the marker
    // type closest to the OTIO marker color.
    int type = -1;
    if (otioMarker->metadata().has_key("kdenlive") && otioMarker->metadata()["kdenlive"].type() == typeid(OTIO_NS::AnyDictionary)) {
        auto otioMetadata = std::any_cast<OTIO_NS::AnyDictionary>(otioMarker->metadata()["kdenlive"]);
        if (otioMetadata.has_key("type") && otioMetadata["type"].type() == typeid(int64_t)) {
            type = std::any_cast<int64_t>(otioMetadata["type"]);
        }
    } else {
        type = fromOtioMarkerColor(otioMarker->color());
    }
    markerModel->addMarker(pos, QString::fromStdString(otioMarker->comment()), type);
}

QString OtioImport::resolveFile(const QString &file, const QFileInfo &timelineFile)
{
    // Check whether it is a URL or file name, and if it is
    // relative to the timeline file.
    const QUrl url(file);
    QFileInfo out(url.isValid() ? url.path() : file);
    if (out.isRelative()) {
        out = QFileInfo(timelineFile.path(), out.filePath());
    }
    return out.filePath();
}
